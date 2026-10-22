#include "pch.h"
#include "Undocumented.h"
#include "Tools.h"
#include "EnumNames.h"
#include "MFTools.h"
#include "FrameGenerator.h"
void AkLog(const wchar_t* fmt, ...);

HRESULT FrameGenerator::EnsureRenderTarget(UINT width, UINT height)
{
	if (!HasD3DManager())
	{
		// create a D2D1 render target from WIC bitmap
		wil::com_ptr_nothrow<ID2D1Factory> d2d1Factory;
		RETURN_IF_FAILED(D2D1CreateFactory(D2D1_FACTORY_TYPE_MULTI_THREADED, IID_PPV_ARGS(&d2d1Factory)));

		wil::com_ptr_nothrow<IWICImagingFactory> wicFactory;
		RETURN_IF_FAILED(CoCreateInstance(CLSID_WICImagingFactory, nullptr, CLSCTX_ALL, IID_PPV_ARGS(&wicFactory)));

		RETURN_IF_FAILED(wicFactory->CreateBitmap(width, height, GUID_WICPixelFormat32bppPBGRA, WICBitmapCacheOnDemand, &_bitmap));

		D2D1_RENDER_TARGET_PROPERTIES props{};
		props.pixelFormat.format = DXGI_FORMAT_B8G8R8A8_UNORM;
		props.pixelFormat.alphaMode = D2D1_ALPHA_MODE_PREMULTIPLIED;
		RETURN_IF_FAILED(d2d1Factory->CreateWicBitmapRenderTarget(_bitmap.get(), props, &_renderTarget));

		RETURN_IF_FAILED(CreateRenderTargetResources(width, height));
	}

	_prevTime = MFGetSystemTime();
	_frame = 0;
	return S_OK;
}

const bool FrameGenerator::HasD3DManager() const
{
	return _texture != nullptr;
}

HRESULT FrameGenerator::SetD3DManager(IUnknown* manager, UINT width, UINT height)
{
	RETURN_HR_IF_NULL(E_POINTER, manager);
	RETURN_HR_IF(E_INVALIDARG, !width || !height);

	RETURN_IF_FAILED(manager->QueryInterface(&_dxgiManager));
	RETURN_IF_FAILED(_dxgiManager->OpenDeviceHandle(&_deviceHandle));

	wil::com_ptr_nothrow<ID3D11Device> device;
	RETURN_IF_FAILED(_dxgiManager->GetVideoService(_deviceHandle, IID_PPV_ARGS(&device)));

	// create a texture/surface to write
	CD3D11_TEXTURE2D_DESC desc
	(
		DXGI_FORMAT_B8G8R8A8_UNORM,
		width,
		height,
		1,
		1,
		D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_RENDER_TARGET
	);
	RETURN_IF_FAILED(device->CreateTexture2D(&desc, nullptr, &_texture));
	wil::com_ptr_nothrow<IDXGISurface> surface;
	RETURN_IF_FAILED(_texture.copy_to(&surface));

	// create a D2D1 render target from 2D GPU surface
	wil::com_ptr_nothrow<ID2D1Factory> d2d1Factory;
	RETURN_IF_FAILED(D2D1CreateFactory(D2D1_FACTORY_TYPE_MULTI_THREADED, IID_PPV_ARGS(&d2d1Factory)));

	auto props = D2D1::RenderTargetProperties
	(
		D2D1_RENDER_TARGET_TYPE_DEFAULT,
		D2D1::PixelFormat(DXGI_FORMAT_UNKNOWN, D2D1_ALPHA_MODE_PREMULTIPLIED)
	);
	RETURN_IF_FAILED(d2d1Factory->CreateDxgiSurfaceRenderTarget(surface.get(), props, &_renderTarget));

	RETURN_IF_FAILED(CreateRenderTargetResources(width, height));

	// create GPU RGB => NV12 converter
	RETURN_IF_FAILED(CoCreateInstance(CLSID_VideoProcessorMFT, nullptr, CLSCTX_ALL, IID_PPV_ARGS(&_converter)));

	wil::com_ptr_nothrow<IMFAttributes> atts;
	RETURN_IF_FAILED(_converter->GetAttributes(&atts));
	TraceMFAttributes(atts.get(), L"VideoProcessorMFT");

	MFT_OUTPUT_STREAM_INFO info{};
	RETURN_IF_FAILED(_converter->GetOutputStreamInfo(0, &info));
	WINTRACE(L"FrameGenerator::SetD3DManager CLSID_VideoProcessorMFT flags:0x%08X size:%u alignment:%u", info.dwFlags, info.cbSize, info.cbAlignment);

	wil::com_ptr_nothrow<IMFMediaType> inputType;
	RETURN_IF_FAILED(MFCreateMediaType(&inputType));
	inputType->SetGUID(MF_MT_MAJOR_TYPE, MFMediaType_Video);
	inputType->SetGUID(MF_MT_SUBTYPE, MFVideoFormat_RGB32);
	MFSetAttributeSize(inputType.get(), MF_MT_FRAME_SIZE, width, height);
	RETURN_IF_FAILED(_converter->SetInputType(0, inputType.get(), 0));

	wil::com_ptr_nothrow<IMFMediaType> outputType;
	RETURN_IF_FAILED(MFCreateMediaType(&outputType));
	outputType->SetGUID(MF_MT_MAJOR_TYPE, MFMediaType_Video);
	outputType->SetGUID(MF_MT_SUBTYPE, MFVideoFormat_NV12);
	MFSetAttributeSize(outputType.get(), MF_MT_FRAME_SIZE, width, height);
	RETURN_IF_FAILED(_converter->SetOutputType(0, outputType.get(), 0));

	// make sure the video processor works on GPU
	RETURN_IF_FAILED(_converter->ProcessMessage(MFT_MESSAGE_SET_D3D_MANAGER, (ULONG_PTR)manager));
	return S_OK;
}

// common to CPU & GPU
HRESULT FrameGenerator::CreateRenderTargetResources(UINT width, UINT height)
{
	assert(_renderTarget);
	RETURN_IF_FAILED(_renderTarget->CreateSolidColorBrush(D2D1::ColorF(1, 1, 1, 1), &_whiteBrush));

	RETURN_IF_FAILED(DWriteCreateFactory(DWRITE_FACTORY_TYPE_SHARED, __uuidof(IDWriteFactory), (IUnknown**)&_dwrite));
	RETURN_IF_FAILED(_dwrite->CreateTextFormat(L"Segoe UI", nullptr, DWRITE_FONT_WEIGHT_NORMAL, DWRITE_FONT_STYLE_NORMAL, DWRITE_FONT_STRETCH_NORMAL, 40, L"", &_textFormat));
	RETURN_IF_FAILED(_textFormat->SetParagraphAlignment(DWRITE_PARAGRAPH_ALIGNMENT_CENTER));
	RETURN_IF_FAILED(_textFormat->SetTextAlignment(DWRITE_TEXT_ALIGNMENT_CENTER));
	_width = width;
	_height = height;
	return S_OK;
}

HRESULT FrameGenerator::Generate(IMFSample* sample, REFGUID format, IMFSample** outSample)
{
	RETURN_HR_IF_NULL(E_POINTER, sample);
	RETURN_HR_IF_NULL(E_POINTER, outSample);
	*outSample = nullptr;

	// Atomic Kinection: copy the latest Kinect colour frame from shared memory (written by AtomicKinection.exe --camfeed)
	// pace output at ~30 fps (the pipeline otherwise pulls frames as fast as we can make them)
	{
		static LARGE_INTEGER freq{}, next{};
		if (!freq.QuadPart) QueryPerformanceFrequency(&freq);
		LARGE_INTEGER now; QueryPerformanceCounter(&now);
		LONGLONG period = freq.QuadPart / 30;
		if (next.QuadPart > now.QuadPart)
		{
			DWORD ms = (DWORD)((next.QuadPart - now.QuadPart) * 1000 / freq.QuadPart);
			if (ms > 0 && ms <= 50) Sleep(ms);
			QueryPerformanceCounter(&now);
		}
		next.QuadPart = (next.QuadPart > now.QuadPart - period ? next.QuadPart : now.QuadPart) + period;
	}
	bool haveFrame = ReadKinectFrame();
	if ((_frame % 300) == 0) AkLog(L"generate frame=%I64u have=%d tick=%I64u", _frame, haveFrame, GetTickCount64());
	if (haveFrame)
	{
		if (HasD3DManager())
		{
			wil::com_ptr_nothrow<ID3D11Device> dev;
			_texture->GetDevice(&dev);
			wil::com_ptr_nothrow<ID3D11DeviceContext> ctx;
			dev->GetImmediateContext(&ctx);
			ctx->UpdateSubresource(_texture.get(), 0, nullptr, _frameCopy.data(), _width * 4, 0);
		}
		else if (_bitmap)
		{
			wil::com_ptr_nothrow<IWICBitmapLock> wl;
			if (SUCCEEDED(_bitmap->Lock(nullptr, WICBitmapLockWrite, &wl)))
			{
				UINT stride = 0, size = 0; WICInProcPointer ptr = nullptr;
				wl->GetStride(&stride); wl->GetDataPointer(&size, &ptr);
				if (ptr) for (UINT y = 0; y < _height; y++) CopyMemory(ptr + (size_t)y * stride, _frameCopy.data() + (size_t)y * _width * 4, _width * 4);
			}
		}
	}
	else if (_renderTarget && _textFormat && _dwrite && _whiteBrush)
	{
		_renderTarget->BeginDraw();
		_renderTarget->Clear(D2D1::ColorF(0.05f, 0.05f, 0.08f, 1));
		const wchar_t* text = L"Atomic Kinection\nWaiting for the Kinect\u2026\n(check it is plugged in and powered)";
		wil::com_ptr_nothrow<IDWriteTextLayout> layout;
		RETURN_IF_FAILED(_dwrite->CreateTextLayout(text, (UINT32)wcslen(text), _textFormat.get(), (FLOAT)_width, (FLOAT)_height, &layout));
		_renderTarget->DrawTextLayout(D2D1::Point2F(0, 0), layout.get(), _whiteBrush.get());
		_renderTarget->EndDraw();
	}

	// build a sample using either D3D/DXGI (GPU) or WIC (CPU)
	wil::com_ptr_nothrow<IMFMediaBuffer> mediaBuffer;
	if (HasD3DManager())
	{
		// remove all existing buffers
		RETURN_IF_FAILED(sample->RemoveAllBuffers());

		// create a buffer from this and add to sample
		RETURN_IF_FAILED(MFCreateDXGISurfaceBuffer(__uuidof(ID3D11Texture2D), _texture.get(), 0, 0, &mediaBuffer));
		RETURN_IF_FAILED(sample->AddBuffer(mediaBuffer.get()));

		// if we're on GPU & format is not RGB, convert using GPU
		if (format == MFVideoFormat_NV12)
		{
			assert(_converter);
			RETURN_IF_FAILED(_converter->ProcessInput(0, sample, 0));

			// let converter build the sample for us, note it works because we gave it the D3DManager
			MFT_OUTPUT_DATA_BUFFER buffer = {};
			DWORD status = 0;
			RETURN_IF_FAILED(_converter->ProcessOutput(0, 1, &buffer, &status));
			*outSample = buffer.pSample;
		}
		else
		{
			sample->AddRef();
			*outSample = sample;
		}

		_frame++;
		return S_OK;
	}

	RETURN_IF_FAILED(sample->GetBufferByIndex(0, &mediaBuffer));
	wil::com_ptr_nothrow<IMF2DBuffer2> buffer2D;
	BYTE* scanline;
	LONG pitch;
	BYTE* start;
	DWORD length;
	RETURN_IF_FAILED(mediaBuffer->QueryInterface(IID_PPV_ARGS(&buffer2D)));
	RETURN_IF_FAILED(buffer2D->Lock2DSize(MF2DBuffer_LockFlags_Write, &scanline, &pitch, &start, &length));

	wil::com_ptr_nothrow<IWICBitmapLock> lock;
	auto hr = _bitmap->Lock(nullptr, WICBitmapLockRead, &lock);
	// now we're using regular COM macros because we want to be sure to unlock (or we could use try/catch)
	if (SUCCEEDED(hr))
	{
		UINT w, h;
		hr = lock->GetSize(&w, &h);
		if (SUCCEEDED(hr))
		{
			UINT wicStride;
			hr = lock->GetStride(&wicStride);
			if (SUCCEEDED(hr))
			{
				UINT wicSize;
				WICInProcPointer wicPointer;
				hr = lock->GetDataPointer(&wicSize, &wicPointer);
				if (SUCCEEDED(hr))
				{
					WINTRACE(L"WIC stride:%u WIC size:%u MF pitch:%u MF length:%u frame:%u format:%s", wicStride, wicSize, pitch, length, _frame, GUID_ToStringW(format).c_str());
					if (format == MFVideoFormat_NV12)
					{
						// note we could use MF's converter too
						hr = RGB32ToNV12(wicPointer, wicSize, wicStride, w, h, scanline, length, pitch);
					}
					else
					{
						hr = (wicSize != length || wicStride != pitch) ? E_FAIL : S_OK;
						if (SUCCEEDED(hr))
						{
							if (assert_true(wicPointer)) // WIC annotation is currently wrong on GetDataPointer wicPointer arg
							{
								CopyMemory(scanline, wicPointer, length);
							}
						}
					}

					if (SUCCEEDED(hr))
					{
						_frame++;
						sample->AddRef();
						*outSample = sample;
					}
				}
			}
		}
		lock.reset();
	}

	buffer2D->Unlock2D();
	return hr;
}


// ---------------- Atomic Kinection shared memory ----------------
#pragma pack(push, 1)
struct AkHeader { UINT32 magic; UINT32 width; UINT32 height; UINT32 stride; volatile LONG seq; UINT32 pad; volatile LONGLONG consumerTick; volatile LONGLONG producerTick; };
#pragma pack(pop)
#define AK_MAGIC 0x314B4B41 // 'AKK1'
#define AK_HEADER 64
#define AK_W 1920
#define AK_H 1080

void AkLog(const wchar_t* fmt, ...)
{
	wchar_t msg[512]; va_list ap; va_start(ap, fmt); _vsnwprintf_s(msg, _TRUNCATE, fmt, ap); va_end(ap);
	SYSTEMTIME st; GetLocalTime(&st);
	wchar_t line[700]; _snwprintf_s(line, _TRUNCATE, L"%02u:%02u:%02u pid=%u %s\r\n", st.wHour, st.wMinute, st.wSecond, GetCurrentProcessId(), msg);
	HANDLE f = CreateFileW(L"C:\\ProgramData\\AtomicKinection\\camsource.log", FILE_APPEND_DATA, FILE_SHARE_READ | FILE_SHARE_WRITE, nullptr, OPEN_ALWAYS, 0, nullptr);
	if (f == INVALID_HANDLE_VALUE) return;
	char utf8[1400]; int n = WideCharToMultiByte(CP_UTF8, 0, line, -1, utf8, sizeof(utf8), nullptr, nullptr);
	DWORD w; if (n > 1) WriteFile(f, utf8, n - 1, &w, nullptr); CloseHandle(f);
}

bool FrameGenerator::EnsureShared()
{
	if (_view) return true;
	static ULONGLONG lastTry = 0;
	auto now = GetTickCount64();
	if (now - lastTry < 1000) return false;
	lastTry = now;
	// the helper (AtomicKinection.exe --camfeed) creates the section in the user's session
	wchar_t name[96];
	DWORD session = WTSGetActiveConsoleSessionId();
	swprintf_s(name, L"Session\\%u\\AtomicKinectionFrame", session);
	const wchar_t* names[] = { name, L"Local\\AtomicKinectionFrame", L"Global\\AtomicKinectionFrame" };
	static int logged = 0;
	for (auto n : names)
	{
		_map = OpenFileMappingW(FILE_MAP_ALL_ACCESS, FALSE, n);
		if (_map) { AkLog(L"opened %s", n); break; }
		if (logged < 3) AkLog(L"open %s failed err=%u", n, GetLastError());
	}
	logged++;
	if (!_map) return false;
	DWORD size = AK_HEADER + AK_W * AK_H * 4;
	_view = (BYTE*)MapViewOfFile(_map, FILE_MAP_ALL_ACCESS, 0, 0, size);
	if (!_view) { CloseHandle(_map); _map = nullptr; return false; }
	_frameCopy.resize((size_t)AK_W * AK_H * 4);
	return true;
}

bool FrameGenerator::ReadKinectFrame()
{
	if (!EnsureShared()) return false;
	auto h = (AkHeader*)_view;
	auto now = (LONGLONG)GetTickCount64();
	InterlockedExchange64(&h->consumerTick, now); // tells the helper someone is watching
	static ULONGLONG lastLog = 0;
	bool ok = h->magic == AK_MAGIC && h->width == _width && h->height == _height && now - h->producerTick <= 2000;
	if (now - lastLog > 5000) { lastLog = now; AkLog(L"read ok=%d magic=%X w=%u/%u h=%u/%u age=%lld gpu=%d", ok, h->magic, h->width, _width, h->height, _height, now - h->producerTick, HasD3DManager()); }
	if (!ok) return false;
	for (int attempt = 0; attempt < 3; attempt++)
	{
		LONG s1 = h->seq;
		if (s1 & 1) { Sleep(2); continue; }
		CopyMemory(_frameCopy.data(), _view + AK_HEADER, _frameCopy.size());
		if (h->seq == s1) return true;
	}
	return true; // possibly torn, still better than nothing
}
