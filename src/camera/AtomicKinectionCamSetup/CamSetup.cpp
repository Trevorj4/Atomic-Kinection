// Atomic Kinection camera setup tool: install | uninstall | list | grab <file.bmp>
#include <windows.h>
#include <mfapi.h>
#include <mfidl.h>
#include <mfreadwrite.h>
#include <mfvirtualcamera.h>
#include <stdio.h>
#include <string>
#pragma comment(lib, "mfplat.lib")
#pragma comment(lib, "mf.lib")
#pragma comment(lib, "mfreadwrite.lib")
#pragma comment(lib, "mfsensorgroup.lib")
#pragma comment(lib, "mfuuid.lib")
#pragma comment(lib, "ole32.lib")

static const wchar_t* CLSID_STR = L"{7D5C2B1E-4A9F-4C3E-9B8A-A70C1E5D2F61}";
static const wchar_t* NAME = L"Atomic Kinection";

static int Fail(const char* what, HRESULT hr) { printf("%s failed: 0x%08X\n", what, (unsigned)hr); return 1; }

static HRESULT CreateCam(IMFVirtualCamera** cam)
{
	return MFCreateVirtualCamera(MFVirtualCameraType_SoftwareCameraSource, MFVirtualCameraLifetime_System,
		MFVirtualCameraAccess_AllUsers, NAME, CLSID_STR, nullptr, 0, cam);
}

static IMFActivate* FindCamera(UINT32* countOut = nullptr, bool print = false)
{
	IMFAttributes* a = nullptr; MFCreateAttributes(&a, 1);
	a->SetGUID(MF_DEVSOURCE_ATTRIBUTE_SOURCE_TYPE, MF_DEVSOURCE_ATTRIBUTE_SOURCE_TYPE_VIDCAP_GUID);
	IMFActivate** devs = nullptr; UINT32 n = 0; IMFActivate* found = nullptr;
	if (SUCCEEDED(MFEnumDeviceSources(a, &devs, &n)))
	{
		for (UINT32 i = 0; i < n; i++)
		{
			WCHAR* name = nullptr; UINT32 len = 0;
			devs[i]->GetAllocatedString(MF_DEVSOURCE_ATTRIBUTE_FRIENDLY_NAME, &name, &len);
			if (print) wprintf(L"camera: %s\n", name ? name : L"?");
			if (name && !found && wcsncmp(name, NAME, wcslen(NAME)) == 0) { found = devs[i]; found->AddRef(); }
			CoTaskMemFree(name); devs[i]->Release();
		}
		CoTaskMemFree(devs);
	}
	a->Release(); if (countOut) *countOut = n;
	return found;
}

static int Grab(const wchar_t* path)
{
	IMFActivate* act = FindCamera();
	if (!act) { printf("Atomic Kinection camera not found\n"); return 2; }
	IMFMediaSource* src = nullptr; HRESULT hr = act->ActivateObject(IID_PPV_ARGS(&src));
	if (FAILED(hr)) return Fail("ActivateObject", hr);
	IMFAttributes* ra = nullptr; MFCreateAttributes(&ra, 1); ra->SetUINT32(MF_SOURCE_READER_ENABLE_VIDEO_PROCESSING, TRUE);
	IMFSourceReader* rd = nullptr; hr = MFCreateSourceReaderFromMediaSource(src, ra, &rd);
	if (FAILED(hr)) return Fail("CreateSourceReader", hr);
	IMFMediaType* t = nullptr; MFCreateMediaType(&t); t->SetGUID(MF_MT_MAJOR_TYPE, MFMediaType_Video); t->SetGUID(MF_MT_SUBTYPE, MFVideoFormat_RGB32);
	hr = rd->SetCurrentMediaType(MF_SOURCE_READER_FIRST_VIDEO_STREAM, nullptr, t);
	if (FAILED(hr)) return Fail("SetCurrentMediaType", hr);
	IMFMediaType* cur = nullptr; rd->GetCurrentMediaType(MF_SOURCE_READER_FIRST_VIDEO_STREAM, &cur);
	UINT32 w = 0, h = 0; MFGetAttributeSize(cur, MF_MT_FRAME_SIZE, &w, &h);
	IMFSample* s = nullptr; int got = 0;
	ULONGLONG t0 = GetTickCount64();
	while (GetTickCount64() - t0 < 8000) // 8 s, keep the last frame so the helper has time to start the Kinect
	{
		DWORD idx, flags; LONGLONG ts; IMFSample* x = nullptr;
		hr = rd->ReadSample(MF_SOURCE_READER_FIRST_VIDEO_STREAM, 0, &idx, &flags, &ts, &x);
		if (FAILED(hr)) return Fail("ReadSample", hr);
		if (x) { if (s) s->Release(); s = x; got++; }
	}
	printf("frames read: %d, size %ux%u\n", got, w, h);
	if (!s) return 3;
	IMFMediaBuffer* b = nullptr; s->ConvertToContiguousBuffer(&b);
	BYTE* p; DWORD len; b->Lock(&p, nullptr, &len);
	BITMAPFILEHEADER fh{ 0x4D42, (DWORD)(sizeof(BITMAPFILEHEADER) + sizeof(BITMAPINFOHEADER) + w * h * 4), 0, 0, sizeof(BITMAPFILEHEADER) + sizeof(BITMAPINFOHEADER) };
	BITMAPINFOHEADER ih{ sizeof(ih), (LONG)w, -(LONG)h, 1, 32, BI_RGB };
	FILE* f = _wfopen(path, L"wb"); fwrite(&fh, sizeof fh, 1, f); fwrite(&ih, sizeof ih, 1, f); fwrite(p, 1, min(len, w * h * 4), f); fclose(f);
	b->Unlock(); printf("saved\n");
	src->Shutdown();
	return 0;
}

int wmain(int argc, wchar_t** argv)
{
	if (argc < 2) { printf("usage: AtomicKinectionCamSetup install|uninstall|list|grab file.bmp\n"); return 1; }
	CoInitializeEx(nullptr, COINIT_MULTITHREADED); MFStartup(MF_VERSION);
	std::wstring cmd = argv[1]; int rc = 0;
	if (cmd == L"install")
	{
		IMFVirtualCamera* cam = nullptr; HRESULT hr = CreateCam(&cam);
		if (FAILED(hr)) return Fail("MFCreateVirtualCamera", hr);
		hr = cam->Start(nullptr);
		if (FAILED(hr)) return Fail("Start", hr);
		printf("installed\n");
		cam->Release(); // system lifetime: stays registered after we exit
	}
	else if (cmd == L"uninstall")
	{
		IMFVirtualCamera* cam = nullptr; HRESULT hr = CreateCam(&cam);
		if (FAILED(hr)) return Fail("MFCreateVirtualCamera", hr);
		hr = cam->Remove(); printf("remove: 0x%08X\n", (unsigned)hr); cam->Release();
	}
	else if (cmd == L"list") { UINT32 n; IMFActivate* a = FindCamera(&n, true); printf("Atomic Kinection %s\n", a ? "FOUND" : "not found"); if (a) a->Release(); }
	else if (cmd == L"grab" && argc > 2) rc = Grab(argv[2]);
	MFShutdown();
	return rc;
}
