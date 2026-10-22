// Atomic Kinection test harness: libfreenect2 colour + Kinect USB audio (IF3) -> VB-CABLE Input (WASAPI)
#define NOMINMAX
#include <windows.h>
#include <mmdeviceapi.h>
#include <audioclient.h>
#include <functiondiscoverykeys_devpkey.h>
#include <stdio.h>
#include <math.h>
#include <atomic>
#include <thread>
#include <vector>
#include <string>
#include <libusb.h>
#include <libfreenect2/libfreenect2.hpp>
#include <libfreenect2/frame_listener_impl.h>
#include <libfreenect2/logger.h>

static FILE* L;
static void Log(const char* f, ...) { SYSTEMTIME t; GetLocalTime(&t); va_list a; va_start(a, f); char b[512]; vsnprintf(b, 512, f, a); va_end(a);
  fprintf(L, "%02d:%02d:%02d.%03d %s\n", t.wHour, t.wMinute, t.wSecond, t.wMilliseconds, b); fflush(L); }

// ---- ring buffer (16 kHz mono float) ----
static const int RB = 16000 * 4; static float ring[RB]; static std::atomic<long long> wpos{0}, rpos{0};
static std::atomic<long long> audBytes{0}, audPkts{0}, audBad{0}, audEmpty{0}; static double gainv = 8.0; static std::atomic<double> inSq{0}; static std::atomic<long long> inN{0};

static void LIBUSB_CALL acb(libusb_transfer* t) {
  for (int i = 0; i < t->num_iso_packets; i++) { auto& p = t->iso_packet_desc[i];
    if (p.status != LIBUSB_TRANSFER_COMPLETED) { audBad++; continue; } audPkts++;
    int n = p.actual_length; if (!n) { audEmpty++; continue; } audBytes += n;
    unsigned char* b = libusb_get_iso_packet_buffer_simple(t, i); int ns = n / 16; long long w = wpos; double sq = 0;
    for (int s = 0; s < ns; s++) { double m = 0; for (int c = 0; c < 4; c++) { int v; memcpy(&v, b + s * 16 + c * 4, 4); m += v / 2147483648.0; }
      m = m / 4 * gainv; if (m > 1) m = 1; if (m < -1) m = -1; sq += m * m; ring[w % RB] = (float)m; w++; }
    wpos = w; inSq = inSq + sq; inN += ns; }
  if (t->status == LIBUSB_TRANSFER_COMPLETED || t->status == LIBUSB_TRANSFER_ERROR) libusb_submit_transfer(t); }

static std::atomic<bool> stop{false};
static IMMDevice* FindDev(EDataFlow flow, const wchar_t* match) {
  IMMDeviceEnumerator* e; CoCreateInstance(__uuidof(MMDeviceEnumerator), 0, CLSCTX_ALL, IID_PPV_ARGS(&e));
  IMMDeviceCollection* col; e->EnumAudioEndpoints(flow, DEVICE_STATE_ACTIVE, &col); UINT n; col->GetCount(&n);
  for (UINT i = 0; i < n; i++) { IMMDevice* d; col->Item(i, &d); IPropertyStore* ps; d->OpenPropertyStore(STGM_READ, &ps); PROPVARIANT v; PropVariantInit(&v);
    ps->GetValue(PKEY_Device_FriendlyName, &v); bool ok = v.pwszVal && wcsstr(v.pwszVal, match); if (ok) { char nm[256]; WideCharToMultiByte(CP_UTF8, 0, v.pwszVal, -1, nm, 256, 0, 0); Log("audio device: %s", nm); return d; } d->Release(); }
  return nullptr; }

static void RenderThread() {
  CoInitializeEx(0, COINIT_MULTITHREADED);
  IMMDevice* d = FindDev(eRender, L"CABLE Input"); if (!d) d = FindDev(eRender, L"VB-Audio Virtual Cable"); if (!d) { Log("no CABLE Input"); return; }
  IAudioClient* ac; d->Activate(__uuidof(IAudioClient), CLSCTX_ALL, 0, (void**)&ac);
  WAVEFORMATEX wf{}; wf.wFormatTag = WAVE_FORMAT_IEEE_FLOAT; wf.nChannels = 1; wf.nSamplesPerSec = 48000; wf.wBitsPerSample = 32; wf.nBlockAlign = 4; wf.nAvgBytesPerSec = 48000 * 4;
  HRESULT hr = ac->Initialize(AUDCLNT_SHAREMODE_SHARED, AUDCLNT_STREAMFLAGS_AUTOCONVERTPCM | AUDCLNT_STREAMFLAGS_SRC_DEFAULT_QUALITY, 400000, 0, &wf, 0);
  if (FAILED(hr)) { Log("render init %08X", hr); return; }
  IAudioRenderClient* rc; ac->GetService(IID_PPV_ARGS(&rc)); UINT32 bufN; ac->GetBufferSize(&bufN); ac->Start(); Log("render started 48k mono, buffer %u", bufN);
  double frac = 0; float last = 0;
  while (!stop) { Sleep(10); UINT32 pad; ac->GetCurrentPadding(&pad); UINT32 want = bufN - pad; if (pad > 48000 / 10) continue; // keep ~100 ms queued
    long long avail = wpos - rpos; if (avail > 16000 / 2) { rpos = wpos - 1600; avail = 1600; } // resync if >0.5 s behind
    UINT32 can = (UINT32)std::min<long long>(want, (avail - 1) * 3); if (avail < 2 || can == 0) continue;
    BYTE* p; if (FAILED(rc->GetBuffer(can, &p))) continue; float* o = (float*)p; long long r = rpos;
    for (UINT32 i = 0; i < can; i++) { float a = ring[r % RB], b = ring[(r + 1) % RB]; o[i] = (float)(a + (b - a) * frac); frac += 1.0 / 3; if (frac >= 1) { frac -= 1; r++; } } // 16k -> 48k linear
    rpos = r; rc->ReleaseBuffer(can, 0); }
  ac->Stop(); }

static std::atomic<double> outSq{0}; static std::atomic<long long> outN{0};
static void MeterThread() { // capture "CABLE Output" to prove the virtual mic carries audio
  CoInitializeEx(0, COINIT_MULTITHREADED); IMMDevice* d = FindDev(eCapture, L"CABLE Output"); if (!d) { Log("no CABLE Output"); return; }
  IAudioClient* ac; d->Activate(__uuidof(IAudioClient), CLSCTX_ALL, 0, (void**)&ac); WAVEFORMATEX* mf; ac->GetMixFormat(&mf);
  if (FAILED(ac->Initialize(AUDCLNT_SHAREMODE_SHARED, 0, 400000, 0, mf, 0))) { Log("meter init failed"); return; }
  IAudioCaptureClient* cc; ac->GetService(IID_PPV_ARGS(&cc)); ac->Start(); Log("meter on CABLE Output: %u Hz %u ch", mf->nSamplesPerSec, mf->nChannels);
  while (!stop) { Sleep(20); UINT32 n; while (SUCCEEDED(cc->GetNextPacketSize(&n)) && n) { BYTE* p; DWORD fl; cc->GetBuffer(&p, &n, &fl, 0, 0);
      if (!(fl & AUDCLNT_BUFFERFLAGS_SILENT)) { float* f = (float*)p; double s = 0; for (UINT32 i = 0; i < n * mf->nChannels; i++) s += f[i] * f[i]; outSq = outSq + s; outN += n * mf->nChannels; } else outN += n * mf->nChannels;
      cc->ReleaseBuffer(n); } } }

static double dB(double sq, long long n) { return n ? 10 * log10(sq / n + 1e-20) : -200; }

int main(int argc, char** argv) {
  int secs = argc > 1 ? atoi(argv[1]) : 120; if (argc > 2) gainv = atof(argv[2]);
  L = fopen("akcombo.log", "a"); Log("=== akcombo start, %d s, gain %.1f", secs, gainv);
  if (getenv("AK_RGB_ONLY")) Log("AK_RGB_ONLY set"); libfreenect2::setGlobalLogger(libfreenect2::createConsoleLogger(libfreenect2::Logger::Warning));
  libfreenect2::Freenect2 fn; if (fn.enumerateDevices() == 0) { Log("no Kinect"); return 1; }
  auto pipe = new libfreenect2::CpuPacketPipeline(); libfreenect2::Freenect2Device* dev = fn.openDefaultDevice(pipe); if (!dev) { Log("open failed"); return 2; }
  libfreenect2::SyncMultiFrameListener lis(libfreenect2::Frame::Color); dev->setColorFrameListener(&lis);
  if (!dev->startStreams(true, false)) { Log("start failed"); return 3; } if (argc > 4) { float ex = (float)atof(argv[4]); dev->setColorSemiAutoExposure(ex); Log("semi-auto exposure %.1f ms", ex); } Log("camera started, serial %s fw %s", dev->getSerialNumber().c_str(), dev->getFirmwareVersion().c_str());
  // audio on our own libusb handle, after camera start (libfreenect2 set the configuration)
  libusb_context* uc; libusb_init(&uc); bool noaud = argc > 3 && !strcmp(argv[3], "noaudio"); auto h = noaud ? nullptr : libusb_open_device_with_vid_pid(uc, 0x045e, 0x02d8); if (!h) Log("audio open failed");
  else { int r1 = libusb_claim_interface(h, 3); libusb_claim_interface(h, 2); libusb_set_interface_alt_setting(h, 3, 0); Sleep(50); int r2 = libusb_set_interface_alt_setting(h, 3, 1); libusb_clear_halt(h, 0x85); Log("audio claim %d alt %d", r1, r2);
    for (int i = 0; i < 8; i++) { auto t = libusb_alloc_transfer(32); libusb_fill_iso_transfer(t, h, 0x85, new unsigned char[256 * 32], 256 * 32, 32, acb, 0, 1000); libusb_set_iso_packet_lengths(t, 256); libusb_submit_transfer(t); } }
  std::thread ev([&] { timeval tv{0, 50000}; while (!stop) libusb_handle_events_timeout(uc, &tv); });
  std::thread rt(RenderThread), mt(MeterThread);
  libfreenect2::FrameMap fm; long long frames = 0, lastF = 0; DWORD t0 = GetTickCount(), lastT = t0; int stalls = 0; long long lastB = 0; double lastIn = 0, lastOut = 0; long long lastInN = 0, lastOutN = 0;
  while (GetTickCount() - t0 < (DWORD)secs * 1000) {
    if (lis.waitForNewFrame(fm, 2000)) { frames++; if (frames == 1) Log("first colour frame"); lis.release(fm); } else { stalls++; Log("colour stall (no frame 2 s)"); }
    DWORD now = GetTickCount(); if (now - lastT >= 10000) { double s = (now - lastT) / 1000.0; double is = inSq, os = outSq; long long in_ = inN, on = outN;
      Log("fps %.1f | audio %.1f kB/s pkts %lld bad %lld empty %lld | mic in %.1f dBFS | CABLE Output %.1f dBFS", (frames - lastF) / s, (audBytes - lastB) / 1024.0 / s, (long long)audPkts, (long long)audBad, (long long)audEmpty,
        dB(is - lastIn, in_ - lastInN), dB(os - lastOut, on - lastOutN));
      lastF = frames; lastT = now; lastB = audBytes; lastIn = is; lastOut = os; lastInN = in_; lastOutN = on; } }
  double tot = (GetTickCount() - t0) / 1000.0;
  Log("RESULT %.0f s colour frames %lld avg fps %.1f stalls %d | audio bytes %lld pkts %lld bad %lld empty %lld | mic %.1f dBFS | CABLE Output %.1f dBFS",
      tot, frames, frames / tot, stalls, (long long)audBytes, (long long)audPkts, (long long)audBad, (long long)audEmpty, dB(inSq, inN), dB(outSq, outN));
  stop = true; rt.join(); mt.join(); ev.join(); dev->stop(); dev->close(); return 0; }
