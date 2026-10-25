// Atomic Kinection 1.3.6 feed helper (akfeed.exe) - freeware, no Microsoft Kinect components.
// Kinect v2 colour / IR / depth via libfreenect2 (libusbK) -> "Atomic Kinection" virtual camera (shared memory),
// Kinect microphone array (USB audio interface 3) -> VB-CABLE "CABLE Input" (appears to apps as "CABLE Output").
#define NOMINMAX
#include <windows.h>
#include <shlobj.h>
#include <sddl.h>
#include <mmdeviceapi.h>
#include <audioclient.h>
#include <functiondiscoverykeys_devpkey.h>
#include <mfapi.h>
#include <mfidl.h>
#include <mfreadwrite.h>
#include <codecapi.h>
#include <stdio.h>
#include <math.h>
#include <atomic>
#include <thread>
#include <string>
#include <vector>
#include <libusb.h>
#include <libfreenect2/libfreenect2.hpp>
#include <libfreenect2/frame_listener_impl.h>
#include <libfreenect2/logger.h>
#include <libfreenect2/packet_pipeline.h>
#include <libfreenect2/depth_packet_processor.h> // internal header: good()
#include <mutex>
#include <algorithm>
#include <opencv2/core.hpp>
#include <opencv2/imgproc.hpp>
#include <opencv2/objdetect.hpp>
#include <exception>
#include "frame_orient.h"
#include "shutdown_order.h"
static std::atomic<bool> quit{false};
// 1.3.4: clean shutdown. The feed stops (and lets go of the Kinect) when the Atomic Kinection window closes
// (the app sets this event), on akfeed.exe --stop, Ctrl+C / console close, sign-out / shutdown (WM_ENDSESSION) and on a crash.
static const wchar_t* QUIT_EVENT = L"Local\\AtomicKinectionFeedQuit";
static HANDLE quitEvt = nullptr, doneEvt = nullptr; // doneEvt: set once the Kinect is closed and threads are joined
static libfreenect2::Freenect2Device* volatile gDev = nullptr; // current device, for the crash handler
static void RequestQuit() { quit = true; if (quitEvt) SetEvent(quitEvt); }
static void Nap(DWORD ms) { if (quitEvt) WaitForSingleObject(quitEvt, ms); else Sleep(ms); } // a Sleep that wakes up on quit

static const int W = 1920, H = 1080, HDR = 64; static const UINT32 MAGIC = 0x314B4B41;
static std::wstring dataDir, docsDir; static FILE* LF;
static void Log(const char* f, ...) { SYSTEMTIME t; GetLocalTime(&t); va_list a; va_start(a, f); char b[600]; vsnprintf(b, sizeof b, f, a); va_end(a);
  if (LF) { fprintf(LF, "%02d/%02d %02d:%02d:%02d.%03d %s\n", t.wDay, t.wMonth, t.wHour, t.wMinute, t.wSecond, t.wMilliseconds, b); fflush(LF); } }

// ---------- libfreenect2 log -> akfeed.log, rate-limited (no console spam); corrupt colour frames are counted and skipped ----------
static std::atomic<float> frExp{0}, frGain{0};
static std::atomic<long long> jpegErrors{0}, lfnMsgs{0};
class AkLogger : public libfreenect2::Logger {
public:
  AkLogger() { level_ = Warning; }
  void log(Level lvl, const std::string& m) override {
    if (m.find("Failed to decompress rgb image") != std::string::npos) { long long n = ++jpegErrors; if (n <= 3) Log("rgb: corrupt JPEG frame skipped (%s)", m.c_str()); return; }
    if (lvl > Warning) return; if (++lfnMsgs <= 200) Log("libfreenect2 %s: %s", level2str(lvl).c_str(), m.c_str()); }
};
static std::atomic<int> mode{0}; static std::atomic<bool> autoFraming{false}, zoomed{false}; // 0 Normal, 1 Night, 2 Depth
// ---------- settings (Documents\AtomicKinection\settings.ini) ----------
static std::atomic<double> framingUp{0.12}; // Auto Framing: raise the crop centre by this fraction of the crop height (FramingOffsetY)
static std::atomic<double> micGainDb{18.0};
// exposure: ExposureMode=semi|auto|manual, ExposureMs (semi max / manual time), AnalogGain 1-4 (manual), ExposureComp -2..2 (auto), Brightness 0.5-4 (digital, gamma-style lift)
static std::string expMode = "auto"; static double expMs = 30, expGain = 2, expComp = 0; static std::atomic<double> brightness{1.0}; static std::atomic<int> expGen{0}; static std::mutex expMx;
static void ReadSettings() { std::lock_guard<std::mutex> lk(expMx);
  FILE* f = _wfopen((docsDir + L"\\settings.ini").c_str(), L"r"); if (!f) return; char ln[256];
  while (fgets(ln, sizeof ln, f)) { char* e = ln + strlen(ln); while (e > ln && (e[-1] == '\n' || e[-1] == '\r' || e[-1] == ' ')) *--e = 0;
    auto num = [&](const char* k, double& v, double lo, double hi) { size_t n = strlen(k); if (!_strnicmp(ln, k, n)) { double x = atof(ln + n); x = std::max(lo, std::min(hi, x)); if (x != v) { v = x; expGen++; } } };
    if (!_strnicmp(ln, "ExposureMode=", 13)) { std::string m = ln + 13; if ((m == "semi" || m == "auto" || m == "manual") && m != expMode) { expMode = m; expGen++; } }
    num("ExposureMs=", expMs, 1, 66); num("AnalogGain=", expGain, 1, 4); num("ExposureComp=", expComp, -2, 2);
    if (!_strnicmp(ln, "Brightness=", 11)) { double b = atof(ln + 11); if (b >= 0.5 && b <= 4) brightness = b; }
    if (!_strnicmp(ln, "Mode=", 5)) mode = !_stricmp(ln + 5, "Night") ? 1 : !_stricmp(ln + 5, "Depth") ? 2 : 0;
    if (!_strnicmp(ln, "AutoFraming=", 12)) autoFraming = atoi(ln + 12) == 1;
    if (!_strnicmp(ln, "Zoom=", 5)) zoomed = !_stricmp(ln + 5, "Zoomed");
    if (!_strnicmp(ln, "FramingOffsetY=", 15)) framingUp = std::max(-0.5, std::min(0.5, atof(ln + 15)));
    if (!_strnicmp(ln, "MicGainDb=", 10)) { double g = atof(ln + 10); if (g >= 0 && g <= 40) micGainDb = g; } } fclose(f); }

// ---------- shared memory (same layout as the camera DLL's AkHeader) ----------
struct AkHeader { UINT32 magic, width, height, stride; volatile LONG seq; UINT32 pad; volatile LONGLONG consumerTick; volatile LONGLONG producerTick; };
static BYTE* view; static AkHeader* hdr;
static bool CreateShared() {
  PSECURITY_DESCRIPTOR sd; ConvertStringSecurityDescriptorToSecurityDescriptorW(L"D:(A;;GA;;;WD)(A;;GA;;;AC)(A;;GA;;;SY)(A;;GA;;;LS)(A;;GA;;;NS)", SDDL_REVISION_1, &sd, nullptr);
  SECURITY_ATTRIBUTES sa{ sizeof sa, sd, FALSE };
  HANDLE m = CreateFileMappingW(INVALID_HANDLE_VALUE, &sa, PAGE_READWRITE, 0, HDR + W * H * 4, L"Local\\AtomicKinectionFrame");
  if (!m) { Log("shared memory failed %u", GetLastError()); return false; }
  view = (BYTE*)MapViewOfFile(m, FILE_MAP_ALL_ACCESS, 0, 0, 0); hdr = (AkHeader*)view; Log("shared memory ready"); return view != nullptr; }
static void Publish() { // caller has filled view+HDR
  hdr->magic = MAGIC; hdr->width = W; hdr->height = H; hdr->stride = W * 4; InterlockedExchange(&hdr->seq, (hdr->seq | 1) + 1);
  InterlockedExchange64(&hdr->producerTick, (LONGLONG)GetTickCount64()); }
static void Stamp() { InterlockedExchange64(&hdr->producerTick, (LONGLONG)GetTickCount64()); }
// ---------- modes: Normal colour / Night vision (IR) / Depth; Auto Framing (YuNet) and Wide/Zoomed crop for all modes ----------
static const char* ModeName(int m) { return m == 1 ? "Night" : m == 2 ? "Depth" : "Normal"; }
static cv::Mat comp(H, W, CV_8UC4); // composed frame of the current mode (true left-right orientation)
static BYTE lut[256]; static double lutFor = -1;
template <class F> static void ParRows(int h, F f) { const int NT = 8; std::thread t[NT - 1]; for (int i = 1; i < NT; i++) t[i - 1] = std::thread(f, h * i / NT, h * (i + 1) / NT); f(0, h / NT); for (auto& x : t) x.join(); }
static void ComposeColour(const BYTE* src) { // sensor order (no left-right flip) + brightness lift
  double br = brightness; if (br != lutFor) { lutFor = br; double gm = 1.0 / br; for (int i = 0; i < 256; i++) lut[i] = (BYTE)std::min(255.0, 255.0 * pow(i / 255.0, gm) + 0.5); }
  BYTE* dst = comp.data;
  ParRows(H, [&](int y0, int y1) { for (int y = y0; y < y1; y++) CopyBgraRow(src + (size_t)y * W * 4, dst + (size_t)y * W * 4, W, kMirrorFeed, lut); }); }
static double irMax = 4000;
// IR and depth are 512x424 float frames. Flip only when kMirrorFeed is set, so they match colour.
static cv::Mat FeedFloat(const libfreenect2::Frame* f) {
  cv::Mat src((int)f->height, (int)f->width, CV_32FC1, f->data);
  if (!kMirrorFeed) return src;
  cv::Mat flipped; cv::flip(src, flipped, 1); return flipped;
}
static void FitInto(const cv::Mat& small) { // 512x424 BGR(A) -> pillar-boxed 1920x1080, high-quality upscale
  int fw = (int)(small.cols * (double)H / small.rows + 0.5); int x0 = (W - fw) / 2; comp.setTo(cv::Scalar(0, 0, 0, 255));
  cv::Mat big; cv::resize(small, big, cv::Size(fw, H), 0, 0, cv::INTER_CUBIC); if (big.channels() == 3) cv::cvtColor(big, big, cv::COLOR_BGR2BGRA); big.copyTo(comp(cv::Rect(x0, 0, fw, H))); }
static void ComposeIr(const libfreenect2::Frame* f) { // same left-right orientation as colour
  cv::Mat ir = FeedFloat(f);
  std::vector<float> smp; smp.reserve(4000); for (int i = 0; i < ir.rows * ir.cols; i += 53) smp.push_back(ir.at<float>(i / ir.cols, i % ir.cols));
  std::nth_element(smp.begin(), smp.begin() + smp.size() * 99 / 100, smp.end()); double p99 = std::max(500.0, (double)smp[smp.size() * 99 / 100]); irMax += (p99 - irMax) * 0.1; // auto level
  cv::Mat g(ir.size(), CV_8UC1); for (int y = 0; y < ir.rows; y++) { const float* s = ir.ptr<float>(y); BYTE* d = g.ptr<BYTE>(y); for (int x = 0; x < ir.cols; x++) d[x] = (BYTE)std::min(255.0, 255.0 * sqrt(std::max(0.0f, s[x]) / irMax)); }
  cv::Mat bgr; cv::cvtColor(g, bgr, cv::COLOR_GRAY2BGR); FitInto(bgr); }
static void ComposeDepth(const libfreenect2::Frame* f) {
  cv::Mat dm = FeedFloat(f);
  cv::Mat g(dm.size(), CV_8UC1), valid(dm.size(), CV_8UC1);
  for (int y = 0; y < dm.rows; y++) { const float* s = dm.ptr<float>(y); BYTE* d = g.ptr<BYTE>(y); BYTE* v = valid.ptr<BYTE>(y);
    for (int x = 0; x < dm.cols; x++) { float mm = s[x]; v[x] = mm > 0 ? 255 : 0; d[x] = (BYTE)(255 - std::max(0.0, std::min(255.0, (mm - 500) * 255.0 / 4000))); } } // near = warm
  cv::Mat col; cv::applyColorMap(g, col, cv::COLORMAP_TURBO); col.setTo(cv::Scalar(0, 0, 0), ~valid); FitInto(col); }

// Auto Framing: YuNet face detector (OpenCV DNN, MIT model) -> head-and-shoulders crop
static const int SW = 640, SH = 360;
static std::mutex smallMx; static cv::Mat smallImg(SH, SW, CV_8UC3); static std::atomic<bool> smallReady{false};
static std::mutex tgtMx; static double tX = W / 2.0, tY = H / 2.0, tH = H; static std::atomic<ULONGLONG> lastFaceTick{0};
static double cX = W / 2.0, cY = H / 2.0, cH = H; static std::atomic<int> faceCount{0}; static std::atomic<bool> modelOk{false};
static std::wstring exeDir;
static void Shrink() { cv::Mat s; cv::resize(comp, s, cv::Size(SW, SH), 0, 0, cv::INTER_AREA); std::lock_guard<std::mutex> g(smallMx); cv::cvtColor(s, smallImg, cv::COLOR_BGRA2BGR); smallReady = true; }
static void OnTerminate();
static void ArmThisThread() { std::set_terminate(OnTerminate); } // MSVC's set_terminate is per thread; fast-fail still skips it
static void FaceThread() {
  ArmThisThread();
  std::string model; { char b[MAX_PATH * 3]; WideCharToMultiByte(CP_UTF8, 0, (exeDir + L"\\models\\face_detection_yunet_2023mar.onnx").c_str(), -1, b, sizeof b, 0, 0); model = b; }
  cv::Ptr<cv::FaceDetectorYN> det;
  try { det = cv::FaceDetectorYN::create(model, "", cv::Size(SW, SH), 0.7f, 0.3f, 50); modelOk = true; Log("Auto Framing face model loaded (YuNet)"); }
  catch (const cv::Exception& e) { Log("Auto Framing: face model failed: %s", e.what()); return; }
  cv::Mat img, faces;
  while (!quit) {
    if (!autoFraming || !smallReady) { Sleep(30); continue; }
    { std::lock_guard<std::mutex> g(smallMx); smallImg.copyTo(img); smallReady = false; }
    try { det->detect(img, faces); } catch (const cv::Exception& e) { Log("face detect error: %s", e.what()); Sleep(500); continue; }
    faceCount = faces.rows; int best = -1; float area = 0;
    for (int i = 0; i < faces.rows; i++) { float a = faces.at<float>(i, 2) * faces.at<float>(i, 3); if (a > area) { area = a; best = i; } }
    if (best >= 0) {
      double k = (double)W / SW, fx = (faces.at<float>(best, 0) + faces.at<float>(best, 2) / 2) * k, fy = (faces.at<float>(best, 1) + faces.at<float>(best, 3) / 2) * k, fh = faces.at<float>(best, 3) * k;
      double h = std::max(H * 0.35, std::min(H * 0.8, fh * 2.8)), cy = fy + h * (0.15 - framingUp); // head-and-shoulders, always a visible crop
      std::lock_guard<std::mutex> g(tgtMx);
      if (fabs(fx - tX) > h * 0.06 || fabs(cy - tY) > h * 0.06 || fabs(h - tH) > h * 0.08 || lastFaceTick == 0) { tX = fx; tY = cy; tH = h; }
      lastFaceTick = GetTickCount64(); }
    Sleep(40); } }
// Night/Depth decode on the graphics card (OpenCL, delay-loaded) when available; otherwise the CPU path.
static bool oclBad = false; static std::atomic<int> gpuDepth{0};
#ifdef LIBFREENECT2_WITH_OPENCL_SUPPORT
static libfreenect2::PacketPipeline* NewOcl() { try { return new libfreenect2::OpenCLPacketPipeline(-1); } catch (...) { return nullptr; } } // OpenCL.dll presence checked first (delay-loaded)
#endif
static libfreenect2::PacketPipeline* MakePipeline(bool wantDepth) {
#ifdef LIBFREENECT2_WITH_OPENCL_SUPPORT
  if (wantDepth && !oclBad) {
    if (!LoadLibraryW(L"OpenCL.dll")) { oclBad = true; Log("OpenCL not available - using CPU depth decoding"); }
    else { libfreenect2::PacketPipeline* p = NewOcl();
      if (p && p->getDepthPacketProcessor() && p->getDepthPacketProcessor()->good()) { if (gpuDepth != 1) Log("depth decoding on the graphics card (OpenCL)"); gpuDepth = 1; return p; }
      delete p; oclBad = true; Log("OpenCL depth decoder failed to start - using CPU depth decoding"); } }
#endif
  if (wantDepth) gpuDepth = 0; return new libfreenect2::CpuPacketPipeline(); }
static void Recorder_Push(const BYTE* frame); // recording tap
static void Output() { // comp -> crop (Auto Framing / Zoomed / Wide) -> shared memory (virtual webcam) + recorder
  double gx = W / 2.0, gy = H / 2.0, gh = H;
  if (autoFraming && modelOk && GetTickCount64() - lastFaceTick < 2000) { std::lock_guard<std::mutex> g(tgtMx); gx = tX; gy = tY; gh = tH; }
  if (zoomed) gh /= 1.6; // Zoomed tightens both the plain view and the Auto Framing crop
  const double a = 0.08; cX += (gx - cX) * a; cY += (gy - cY) * a; cH += (gh - cH) * a;
  double ch = std::max(H * 0.35, std::min((double)H, cH)), cw = ch * W / H;
  InterlockedExchange(&hdr->seq, hdr->seq | 1); cv::Mat d(H, W, CV_8UC4, view + HDR);
  if (ch >= H - 1) comp.copyTo(d);
  else { double x0 = std::max(0.0, std::min(W - cw, cX - cw / 2)), y0 = std::max(0.0, std::min(H - ch, cY - ch / 2));
    cv::Rect r((int)x0, (int)y0, (int)cw, (int)ch); r &= cv::Rect(0, 0, W, H); cv::resize(comp(r), d, cv::Size(W, H), 0, 0, cv::INTER_CUBIC); }
  Publish(); Recorder_Push(view + HDR); }
static void Banner() { // dim the last frame + bar so the camera shows "reconnecting" instead of freezing silently
  BYTE* p = view + HDR; for (int y = H - 140; y < H - 60; y++) for (int x = 0; x < W; x++) { BYTE* q = p + (y * W + x) * 4; q[0] /= 4; q[1] /= 4; q[2] = (BYTE)(q[2] / 4 + 120); } Publish(); }

// ---------- Recording: exactly what the webcam shows + Kinect mic -> MP4 (H.264 + AAC, Windows Media Foundation) ----------
// The app creates %LOCALAPPDATA%\AtomicKinection\record.flag to start and deletes it to stop.
static std::atomic<bool> recOn{false}; static std::atomic<double> recSecs{0}; static std::wstring recFile; static std::mutex recFileMx;
static std::mutex rqMx; static std::vector<std::vector<BYTE>> rqPool; static std::vector<std::pair<std::vector<BYTE>, LONGLONG>> rq; static std::atomic<long long> recDropped{0};
static LARGE_INTEGER qpf; static LONGLONG recT0;
static const int RAB = 16000 * 8; static float recRing[RAB]; static std::atomic<long long> recW{0};
static void Recorder_Audio(float m) { long long w = recW; recRing[w % RAB] = m; recW = w + 1; }
static void Recorder_Push(const BYTE* frame) {
  if (!recOn) return; LARGE_INTEGER t; QueryPerformanceCounter(&t);
  std::lock_guard<std::mutex> g(rqMx); if (rq.size() >= 12) { recDropped++; return; }
  std::vector<BYTE> b; if (!rqPool.empty()) { b.swap(rqPool.back()); rqPool.pop_back(); } b.resize(W * H * 4); memcpy(b.data(), frame, W * H * 4);
  rq.emplace_back(std::move(b), (t.QuadPart - recT0) * 10000000 / qpf.QuadPart); }
static bool RecFlag() { return GetFileAttributesW((dataDir + L"\\record.flag").c_str()) != INVALID_FILE_ATTRIBUTES; }
static void RecorderThread() {
  ArmThisThread(); CoInitializeEx(0, COINIT_MULTITHREADED); MFStartup(MF_VERSION); QueryPerformanceFrequency(&qpf);
  while (!quit) {
    if (!RecFlag()) { Sleep(100); continue; }
    PWSTR p; SHGetKnownFolderPath(FOLDERID_Videos, 0, nullptr, &p); std::wstring dir = std::wstring(p) + L"\\Atomic Kinection"; CoTaskMemFree(p); CreateDirectoryW(dir.c_str(), nullptr);
    SYSTEMTIME st; GetLocalTime(&st); wchar_t nm[80]; swprintf(nm, 80, L"\\Atomic Kinection %04d-%02d-%02d %02d-%02d-%02d.mp4", st.wYear, st.wMonth, st.wDay, st.wHour, st.wMinute, st.wSecond);
    std::wstring file = dir + nm;
    IMFAttributes* at = nullptr; MFCreateAttributes(&at, 2); at->SetUINT32(MF_READWRITE_ENABLE_HARDWARE_TRANSFORMS, TRUE); at->SetUINT32(MF_SINK_WRITER_DISABLE_THROTTLING, TRUE);
    IMFSinkWriter* w = nullptr; HRESULT hr = MFCreateSinkWriterFromURL(file.c_str(), nullptr, at, &w); at->Release();
    DWORD vs = 0, as = 0; IMFMediaType* t = nullptr;
    auto vtype = [&](GUID sub, bool out) { MFCreateMediaType(&t); t->SetGUID(MF_MT_MAJOR_TYPE, MFMediaType_Video); t->SetGUID(MF_MT_SUBTYPE, sub);
      t->SetUINT32(MF_MT_INTERLACE_MODE, MFVideoInterlace_Progressive); MFSetAttributeSize(t, MF_MT_FRAME_SIZE, W, H); MFSetAttributeRatio(t, MF_MT_FRAME_RATE, 30, 1); MFSetAttributeRatio(t, MF_MT_PIXEL_ASPECT_RATIO, 1, 1);
      if (out) { t->SetUINT32(MF_MT_AVG_BITRATE, 16000000); t->SetUINT32(MF_MT_MPEG2_PROFILE, eAVEncH264VProfile_High); } else t->SetUINT32(MF_MT_DEFAULT_STRIDE, (UINT32)(W * 4)); };
    auto atype = [&](GUID sub, bool out) { MFCreateMediaType(&t); t->SetGUID(MF_MT_MAJOR_TYPE, MFMediaType_Audio); t->SetGUID(MF_MT_SUBTYPE, sub);
      t->SetUINT32(MF_MT_AUDIO_BITS_PER_SAMPLE, 16); t->SetUINT32(MF_MT_AUDIO_SAMPLES_PER_SECOND, 48000); t->SetUINT32(MF_MT_AUDIO_NUM_CHANNELS, 1);
      if (out) t->SetUINT32(MF_MT_AUDIO_AVG_BYTES_PER_SECOND, 24000); else { t->SetUINT32(MF_MT_AUDIO_BLOCK_ALIGNMENT, 2); t->SetUINT32(MF_MT_AUDIO_AVG_BYTES_PER_SECOND, 96000); } };
    if (SUCCEEDED(hr)) { vtype(MFVideoFormat_H264, true); hr = w->AddStream(t, &vs); t->Release(); }
    if (SUCCEEDED(hr)) { vtype(MFVideoFormat_RGB32, false); hr = w->SetInputMediaType(vs, t, nullptr); t->Release(); }
    if (SUCCEEDED(hr)) { atype(MFAudioFormat_AAC, true); hr = w->AddStream(t, &as); t->Release(); }
    if (SUCCEEDED(hr)) { atype(MFAudioFormat_PCM, false); hr = w->SetInputMediaType(as, t, nullptr); t->Release(); }
    if (SUCCEEDED(hr)) hr = w->BeginWriting();
    if (FAILED(hr)) { Log("recording: could not start (0x%08X)", (unsigned)hr); if (w) w->Release(); DeleteFileW((dataDir + L"\\record.flag").c_str()); Sleep(500); continue; }
    { std::lock_guard<std::mutex> g(recFileMx); recFile = file; }
    { std::lock_guard<std::mutex> g(rqMx); rq.clear(); } LARGE_INTEGER t0; QueryPerformanceCounter(&t0); recT0 = t0.QuadPart; long long aR = recW; long long aOut = 0; double frac = 0; recDropped = 0;
    recOn = true; char fn8[600]; WideCharToMultiByte(CP_UTF8, 0, file.c_str(), -1, fn8, sizeof fn8, 0, 0); Log("recording started: %s", fn8);
    LONGLONG lastV = -1; long long vFrames = 0;
    while (!quit) {
      bool stop = !RecFlag();
      std::vector<std::pair<std::vector<BYTE>, LONGLONG>> batch; { std::lock_guard<std::mutex> g(rqMx); batch.swap(rq); }
      for (auto& fr : batch) {
        LONGLONG ts = fr.second; if (ts <= lastV) ts = lastV + 1; lastV = ts;
        IMFMediaBuffer* mb; MFCreateMemoryBuffer(W * H * 4, &mb); BYTE* d; mb->Lock(&d, nullptr, nullptr); memcpy(d, fr.first.data(), W * H * 4); mb->Unlock(); mb->SetCurrentLength(W * H * 4);
        IMFSample* s; MFCreateSample(&s); s->AddBuffer(mb); s->SetSampleTime(ts); s->SetSampleDuration(333333); w->WriteSample(vs, s); s->Release(); mb->Release(); vFrames++;
        std::lock_guard<std::mutex> g(rqMx); rqPool.push_back(std::move(fr.first)); }
      // audio: 16 kHz mono ring -> 48 kHz 16-bit PCM, timestamped by sample count (starts with the video)
      long long avail = recW - aR; if (avail > RAB / 2) { aR = recW - 1600; avail = 1600; }
      if (avail > 1) { int n = (int)((avail - 1) * 3); IMFMediaBuffer* mb; MFCreateMemoryBuffer(n * 2, &mb); BYTE* d; mb->Lock(&d, nullptr, nullptr); short* o = (short*)d; int k = 0;
        while (k < n && aR + 1 < recW) { float a = recRing[aR % RAB], b = recRing[(aR + 1) % RAB]; o[k++] = (short)std::max(-32767.0, std::min(32767.0, (a + (b - a) * frac) * 32767)); frac += 1.0 / 3; if (frac >= 1) { frac -= 1; aR++; } }
        mb->Unlock(); mb->SetCurrentLength(k * 2); IMFSample* s; MFCreateSample(&s); s->AddBuffer(mb); s->SetSampleTime(aOut * 10000000 / 48000); s->SetSampleDuration((LONGLONG)k * 10000000 / 48000); w->WriteSample(as, s); s->Release(); mb->Release(); aOut += k; }
      LARGE_INTEGER now; QueryPerformanceCounter(&now); recSecs = (double)(now.QuadPart - recT0) / qpf.QuadPart;
      if (stop) break; Sleep(10); }
    recOn = false; w->Finalize(); w->Release(); Log("recording saved: %s (%.1f s, %lld video frames, %.1f s audio, %lld frames skipped)", fn8, (double)recSecs, vFrames, aOut / 48000.0, (long long)recDropped);
    { std::lock_guard<std::mutex> g(recFileMx); recFile.clear(); } recSecs = 0; }
  MFShutdown(); }

// ---------- audio: Kinect IF3 (16 kHz, 4 ch, 32-bit) -> mono -> 48 kHz -> CABLE Input ----------
static const int RB = 16000 * 4; static float ring[RB]; static std::atomic<long long> wpos{0}, rpos{0}; static std::atomic<long long> audPkts{0}, audBytes{0};
static std::atomic<double> levelSq{0}; static std::atomic<long long> levelN{0};
static std::atomic<bool> micStop{false};
static std::atomic<int> micOutstanding{0}; // transfers submitted and not yet completed for the last time
static void LIBUSB_CALL acb(libusb_transfer* t) {
  double g = pow(10.0, micGainDb / 20.0);
  for (int i = 0; i < t->num_iso_packets; i++) { auto& p = t->iso_packet_desc[i]; if (p.status != LIBUSB_TRANSFER_COMPLETED || !p.actual_length) continue;
    audPkts++; audBytes += p.actual_length; unsigned char* b = libusb_get_iso_packet_buffer_simple(t, i); int ns = p.actual_length / 16; long long w = wpos; double sq = 0;
    for (int s = 0; s < ns; s++) { double m = 0; for (int c = 0; c < 4; c++) { int v; memcpy(&v, b + s * 16 + c * 4, 4); m += v / 2147483648.0; }
      m = m / 4 * g; if (m > 1) m = 1; if (m < -1) m = -1; sq += m * m; ring[w % RB] = (float)m; w++; Recorder_Audio((float)m); }
    wpos = w; levelSq = levelSq + sq; levelN += ns; }
  // Keep the transfer alive until shutdown. A cancel, or micStop, is the last callback:
  // Freenect2's USB thread runs that callback, then the count drops and the main thread may free it.
  if (!micStop.load() && (t->status == LIBUSB_TRANSFER_COMPLETED || t->status == LIBUSB_TRANSFER_ERROR) && libusb_submit_transfer(t) == 0) return;
  micOutstanding--; }

static IMMDevice* FindRender(const wchar_t* match) {
  IMMDeviceEnumerator* e = nullptr; if (FAILED(CoCreateInstance(__uuidof(MMDeviceEnumerator), 0, CLSCTX_ALL, IID_PPV_ARGS(&e)))) return nullptr;
  IMMDeviceCollection* col; e->EnumAudioEndpoints(eRender, DEVICE_STATE_ACTIVE, &col); UINT n; col->GetCount(&n);
  for (UINT i = 0; i < n; i++) { IMMDevice* d; col->Item(i, &d); IPropertyStore* ps; d->OpenPropertyStore(STGM_READ, &ps); PROPVARIANT v; PropVariantInit(&v);
    ps->GetValue(PKEY_Device_FriendlyName, &v); bool ok = v.pwszVal && wcsstr(v.pwszVal, match); PropVariantClear(&v); ps->Release(); if (ok) return d; d->Release(); }
  return nullptr; }
static void RenderThread() {
  ArmThisThread(); CoInitializeEx(0, COINIT_MULTITHREADED);
  while (!quit) {
    IMMDevice* d = FindRender(L"CABLE Input"); if (!d) { static int once = 0; if (!once++) Log("VB-CABLE 'CABLE Input' not found - mic output waits for it"); Nap(3000); continue; }
    IAudioClient* ac; d->Activate(__uuidof(IAudioClient), CLSCTX_ALL, 0, (void**)&ac);
    WAVEFORMATEX wf{}; wf.wFormatTag = WAVE_FORMAT_IEEE_FLOAT; wf.nChannels = 1; wf.nSamplesPerSec = 48000; wf.wBitsPerSample = 32; wf.nBlockAlign = 4; wf.nAvgBytesPerSec = 48000 * 4;
    if (FAILED(ac->Initialize(AUDCLNT_SHAREMODE_SHARED, AUDCLNT_STREAMFLAGS_AUTOCONVERTPCM | AUDCLNT_STREAMFLAGS_SRC_DEFAULT_QUALITY, 400000, 0, &wf, 0))) { ac->Release(); d->Release(); Nap(3000); continue; }
    IAudioRenderClient* rc; ac->GetService(IID_PPV_ARGS(&rc)); UINT32 bufN; ac->GetBufferSize(&bufN); ac->Start(); Log("mic output -> CABLE Input (48 kHz mono)");
    double frac = 0; bool bad = false;
    while (!quit && !bad) { Sleep(10); UINT32 pad; if (FAILED(ac->GetCurrentPadding(&pad))) { bad = true; break; } if (pad > 4800) continue;
      long long avail = wpos - rpos; if (avail > 8000) { rpos = wpos - 1600; avail = 1600; }
      UINT32 can = (UINT32)std::min<long long>(bufN - pad, avail > 1 ? (avail - 1) * 3 : 0);
      if (!can) { if (pad < 480) { BYTE* p; if (SUCCEEDED(rc->GetBuffer(480, &p))) rc->ReleaseBuffer(480, AUDCLNT_BUFFERFLAGS_SILENT); } continue; } // keep the device fed with silence when the Kinect is away
      BYTE* p; if (FAILED(rc->GetBuffer(can, &p))) { bad = true; break; } float* o = (float*)p; long long r = rpos;
      for (UINT32 i = 0; i < can; i++) { float a = ring[r % RB], b = ring[(r + 1) % RB]; o[i] = (float)(a + (b - a) * frac); frac += 1.0 / 3; if (frac >= 1) { frac -= 1; r++; } }
      rpos = r; rc->ReleaseBuffer(can, 0); }
    ac->Stop(); rc->Release(); ac->Release(); d->Release(); if (!quit) Log("mic output restarting"); Nap(1000); } }

// ---------- status file for the UI ----------
static std::atomic<double> curFps{0}; static std::atomic<int> kinectOk{0}; static std::atomic<long long> drops{0}, reconnects{0}; static std::atomic<int> activeMode{0};
static void WriteStatus() {
  double sq = levelSq.exchange(0); long long n = levelN.exchange(0); double db = n ? 10 * log10(sq / n + 1e-20) : -120;
  std::wstring rf; { std::lock_guard<std::mutex> g(recFileMx); rf = recFile; } char rf8[600]; WideCharToMultiByte(CP_UTF8, 0, rf.c_str(), -1, rf8, sizeof rf8, 0, 0);
  FILE* f = _wfopen((dataDir + L"\\status.txt").c_str(), L"w"); if (!f) return;
  fprintf(f, "GpuDepth=%d\nKinect=%s\nFps=%.1f\nMode=%s\nAutoFraming=%s\nZoom=%s\nFaces=%d\nMicLevelDb=%.1f\nMicGainDb=%.1f\nDrops=%lld\nReconnects=%lld\nCorruptFrames=%lld\nExposureMs=%.1f\nRecording=%d\nRecSeconds=%.1f\nRecFile=%s\nResolution=%dx%d\nTick=%llu\n",
    (int)gpuDepth, kinectOk ? "OK" : "Reconnecting", (double)curFps, ModeName(activeMode), !autoFraming ? "Off" : modelOk ? "On" : "NoModel", zoomed ? "Zoomed" : "Wide", (int)faceCount, db, (double)micGainDb,
    (long long)drops, (long long)reconnects, (long long)jpegErrors, (double)frExp, (int)recOn, (double)recSecs, rf8, W, H, GetTickCount64()); fclose(f); }

// ---------- 1.3.4 shutdown handlers ----------
static void WaitDone(DWORD ms) { if (doneEvt) WaitForSingleObject(doneEvt, ms); }
static BOOL WINAPI CtrlHandler(DWORD t) { Log("console event %u - shutting down", (unsigned)t); RequestQuit(); WaitDone(8000); return TRUE; } // Ctrl+C, Ctrl+Break, console close, logoff, shutdown
static LRESULT CALLBACK HiddenWndProc(HWND h, UINT m, WPARAM w, LPARAM l) {
  switch (m) {
  case WM_QUERYENDSESSION: return TRUE;
  case WM_ENDSESSION: if (w) { Log("Windows is signing out / shutting down - releasing the Kinect"); RequestQuit(); WaitDone(4000); } return 0;
  case WM_CLOSE: Log("WM_CLOSE - shutting down"); RequestQuit(); return 0; // taskkill (without /f) sends this
  }
  return DefWindowProcW(h, m, w, l); }
static void WindowThread() { // hidden top-level window: message-only windows do not get WM_ENDSESSION
  ArmThisThread();
  WNDCLASSW wc{}; wc.lpfnWndProc = HiddenWndProc; wc.hInstance = GetModuleHandleW(nullptr); wc.lpszClassName = L"AtomicKinectionFeedWnd"; RegisterClassW(&wc);
  HWND h = CreateWindowExW(0, wc.lpszClassName, L"Atomic Kinection feed", WS_OVERLAPPED, 0, 0, 0, 0, nullptr, nullptr, wc.hInstance, nullptr);
  if (!h) return; MSG msg;
  while (!quit) { if (MsgWaitForMultipleObjects(1, &quitEvt, FALSE, INFINITE, QS_ALLINPUT) == WAIT_OBJECT_0) break;
    while (PeekMessageW(&msg, nullptr, 0, 0, PM_REMOVE)) { TranslateMessage(&msg); DispatchMessageW(&msg); } }
  DestroyWindow(h); }
static std::atomic<int> crashing{0};
static void EmergencyRelease() { if (crashing++) return; quit = true; // best effort: stop the Kinect so it isn't left streaming, then end the process (Windows frees the USB handles)
  libfreenect2::Freenect2Device* d = gDev; gDev = nullptr; if (d) { __try { d->stop(); d->close(); } __except (EXCEPTION_EXECUTE_HANDLER) {} } }
static LONG WINAPI CrashFilter(EXCEPTION_POINTERS* e) { Log("crash 0x%08X - releasing the Kinect and exiting", e && e->ExceptionRecord ? (unsigned)e->ExceptionRecord->ExceptionCode : 0u);
  EmergencyRelease(); TerminateProcess(GetCurrentProcess(), 3); return EXCEPTION_EXECUTE_HANDLER; }
static void OnTerminate() { Log("unhandled C++ exception - releasing the Kinect and exiting"); EmergencyRelease(); TerminateProcess(GetCurrentProcess(), 3); }
static int StopRunningFeed() { // akfeed.exe --stop
  HANDLE e = OpenEventW(EVENT_MODIFY_STATE, FALSE, QUIT_EVENT); if (!e) return 0; SetEvent(e); CloseHandle(e);
  for (int i = 0; i < 100; i++) { HANDLE m = OpenMutexW(SYNCHRONIZE, FALSE, L"Local\\AtomicKinectionCamFeed"); if (!m) return 0; CloseHandle(m); Sleep(100); }
  return 1; }

int wmain(int argc, wchar_t** argv) {
  for (int i = 1; i < argc; i++) if (!_wcsicmp(argv[i], L"--stop")) return StopRunningFeed();
  HANDLE mtx = CreateMutexW(nullptr, TRUE, L"Local\\AtomicKinectionCamFeed"); if (GetLastError() == ERROR_ALREADY_EXISTS) return 0;
  quitEvt = CreateEventW(nullptr, TRUE, FALSE, QUIT_EVENT); ResetEvent(quitEvt); doneEvt = CreateEventW(nullptr, TRUE, FALSE, nullptr);
  SetUnhandledExceptionFilter(CrashFilter); std::set_terminate(OnTerminate); SetConsoleCtrlHandler(CtrlHandler, TRUE);
  SetProcessShutdownParameters(0x3FF, SHUTDOWN_NORETRY); // asked to close early at sign-out, before the apps using the webcam
  std::thread qw([] { ArmThisThread(); WaitForSingleObject(quitEvt, INFINITE); quit = true; }), wt(WindowThread);
  PWSTR p; SHGetKnownFolderPath(FOLDERID_LocalAppData, 0, nullptr, &p); dataDir = std::wstring(p) + L"\\AtomicKinection"; CoTaskMemFree(p); CreateDirectoryW(dataDir.c_str(), nullptr);
  SHGetKnownFolderPath(FOLDERID_Documents, 0, nullptr, &p); docsDir = std::wstring(p) + L"\\AtomicKinection"; CoTaskMemFree(p); CreateDirectoryW(docsDir.c_str(), nullptr);
  { wchar_t b[MAX_PATH]; GetModuleFileNameW(nullptr, b, MAX_PATH); exeDir = b; exeDir = exeDir.substr(0, exeDir.find_last_of(L'\\')); }
  DeleteFileW((dataDir + L"\\record.flag").c_str());
  LF = _wfopen((dataDir + L"\\akfeed.log").c_str(), L"a"); Log("akfeed 1.3.6 started (libfreenect2: colour/IR/depth + Kinect mic + YuNet Auto Framing + MP4 recording; no Microsoft Kinect components)");
  ReadSettings(); if (!CreateShared()) { RequestQuit(); qw.join(); wt.join(); return 1; }
  static AkLogger akl; libfreenect2::setGlobalLogger(&akl);
  std::thread rt(RenderThread), ft(FaceThread), rec(RecorderThread);
  std::thread st([] { ArmThisThread(); long long lastJ = 0; int sec = 0; while (!quit) { Sleep(500); int m = mode; bool af = autoFraming, z = zoomed; ReadSettings();
    if (m != mode || af != autoFraming || z != zoomed) Log("view: %s, Auto Framing %s, %s", ModeName(mode), autoFraming ? "on" : "off", zoomed ? "Zoomed" : "Wide"); WriteStatus();
    if (++sec % 120 == 0) { long long j = jpegErrors; if (j != lastJ) Log("rgb: %lld corrupt JPEG frames skipped in the last minute (total %lld)", j - lastJ, j); lastJ = j; } } });
  bool planned = false;
  // One libusb context for the whole process. Freenect2 is given this context so it
  // does not call libusb_exit. The mic submits on the same context and is pumped by
  // Freenect2's USB thread (a second libusb_handle_events on one context is illegal).
  // Windows libusb's backend exit is process-global: a second libusb_exit deletes the
  // autoclaim critical section again and fail-fasts with 0xc0000409. That ran in
  // ~Freenect2, after "Kinect released" and before "joining threads". See shutdown_order.h.
  libusb_context* feedCtx = nullptr;
  while (!quit) {
    // ---- connect: colour only in Normal; colour+IR+depth in Night/Depth ----
    // With OpenCL, colour + IR + depth all stream all the time, so view switches never restart the Kinect
    // (closing the OpenCL decoder on a switch crashed 1.3.2-dev). CPU fallback: depth only in Night/Depth.
    int m = mode; bool wantDepth = true;
    if (!feedCtx && libusb_init(&feedCtx) != 0) { Log("libusb_init failed"); kinectOk = 0; Nap(2000); continue; }
    bool started = false;
    {
      libfreenect2::Freenect2 fn(feedCtx); libfreenect2::Freenect2Device* dev = nullptr;
      if (fn.enumerateDevices() > 0) { libfreenect2::PacketPipeline* pp = MakePipeline(true); if (gpuDepth != 1) { delete pp; wantDepth = m != 0; pp = MakePipeline(wantDepth); } dev = fn.openDefaultDevice(pp); }
      if (!dev) { kinectOk = 0; if (hdr->magic == MAGIC) Stamp(); }
      else {
        const bool always = gpuDepth == 1;
        {
          // Listeners die at the end of this block, before delete dev, so OpenCL frames
          // return their buffers while the allocator and command queue still exist.
          libfreenect2::SyncMultiFrameListener lisC(libfreenect2::Frame::Color), lisD(libfreenect2::Frame::Ir | libfreenect2::Frame::Depth);
          if (always || !wantDepth) dev->setColorFrameListener(&lisC); if (wantDepth) dev->setIrAndDepthFrameListener(&lisD);
          if (!dev->startStreams(true, wantDepth)) { Log("start failed"); dev->close(); }
          else {
            started = true; gDev = dev; activeMode = m;
            int appliedGen = -1;
            auto applyExposure = [&] { std::lock_guard<std::mutex> lk(expMx); appliedGen = expGen;
              if (expMode == "auto") dev->setColorAutoExposure((float)expComp);
              else if (expMode == "manual") dev->setColorManualExposure((float)std::min(expMs, 33.0), (float)expGain);
              else dev->setColorSemiAutoExposure((float)expMs);
              Log("exposure %s ms=%.0f gain=%.1f comp=%.1f brightness=%.2f", expMode.c_str(), expMs, expGain, expComp, (double)brightness); };
            applyExposure();
            Log("Kinect connected (%s view), serial %s firmware %s", ModeName(m), dev->getSerialNumber().c_str(), dev->getFirmwareVersion().c_str());
            libusb_device_handle* ah = nullptr; std::vector<libusb_transfer*> xs;
            if ((ah = libusb_open_device_with_vid_pid(feedCtx, 0x045e, 0x02d8))) {
              int r1 = libusb_claim_interface(ah, 3); libusb_claim_interface(ah, 2); libusb_set_interface_alt_setting(ah, 3, 0); Sleep(50);
              int r2 = libusb_set_interface_alt_setting(ah, 3, 1); libusb_clear_halt(ah, 0x85);
              micStop = false;
              for (int i = 0; i < 8; i++) { auto t = libusb_alloc_transfer(32); libusb_fill_iso_transfer(t, ah, 0x85, new unsigned char[256 * 32], 256 * 32, 32, acb, 0, 1000); libusb_set_iso_packet_lengths(t, 256);
                if (libusb_submit_transfer(t) == 0) micOutstanding++; xs.push_back(t); }
              Log("Kinect mic streaming (claim %d alt %d)", r1, r2);
            } else Log("Kinect mic open failed");
            kinectOk = 1; long long frames = 0, lastF = 0; ULONGLONG lastT = GetTickCount64(); long long lastPk = audPkts, lastJe = jpegErrors; libfreenect2::FrameMap fm; bool lost = false; planned = false;
            while (!quit && !lost) {
              if (appliedGen != expGen) applyExposure();
              if (!always && (mode != 0) != wantDepth) { Log("switching to %s view", ModeName(mode)); planned = true; break; }
              if (mode != activeMode) activeMode = (int)mode; // no restart needed
              const bool col = activeMode == 0;
              if (always) { libfreenect2::FrameMap om; auto& other = col ? lisD : lisC; if (other.hasNewFrame()) { other.waitForNewFrame(om, 0); other.release(om); } }
              auto& lis = col ? lisC : lisD;
              if (lis.waitForNewFrame(fm, 3000)) {
                if (col) { auto c = fm[libfreenect2::Frame::Color]; if (c->width == W && c->height == H && c->bytes_per_pixel == 4) { ComposeColour(c->data); frExp = c->exposure; frGain = c->gain; } }
                else if (activeMode == 1) ComposeIr(fm[libfreenect2::Frame::Ir]); else ComposeDepth(fm[libfreenect2::Frame::Depth]);
                lis.release(fm); Output(); if (autoFraming && (frames % 2) == 0) Shrink(); frames++; }
              else { Log("DROP: no %s frame for 3 s", col ? "colour" : "depth"); lost = true; }
              ULONGLONG now = GetTickCount64(); if (now - lastT >= 2000) { curFps = (frames - lastF) * 1000.0 / (now - lastT); lastF = frames; lastT = now;
                long long je = jpegErrors; if (je - lastJe >= 20) { Log("rgb: %lld corrupt frames in 2 s - restarting the Kinect stream", je - lastJe); lost = true; } lastJe = je;
                long long pk = audPkts; if (pk == lastPk) Log("mic: no audio packets for 2 s"); lastPk = pk; }
            }
            if (!planned) { drops++; kinectOk = 0; Banner(); }
            micStop = true;
            for (auto t : xs) libusb_cancel_transfer(t);
            for (int i = 0; i < 50 && micOutstanding.load() > 0; i++) Sleep(10); // Freenect2's USB thread runs the final callbacks
            if (micOutstanding.load() > 0) Log("mic: %d transfers still outstanding - freeing them anyway", micOutstanding.load());
            for (auto t : xs) { delete[] t->buffer; libusb_free_transfer(t); } xs.clear();
            if (ah) { libusb_release_interface(ah, 3); libusb_release_interface(ah, 2); libusb_close(ah); }
            gDev = nullptr; dev->stop(); dev->close();
          }
        } // release listener frames (including leftover OpenCL IR/depth) before the pipeline dies
        delete dev;
      }
    } // ~Freenect2 stops the USB event thread and does not call libusb_exit
    if (!started) { Nap(2000); continue; }
    Log(quit ? "Kinect released (streams stopped, device closed)" : "Kinect closed");
    if (quit) break;
    if (!planned) { reconnects++; Log("reconnecting (#%lld)", (long long)reconnects); Nap(1000); }
  }
  if (feedCtx) { libusb_exit(feedCtx); feedCtx = nullptr; } // exactly once, after the event thread has stopped
  RequestQuit(); Log("shutting down: joining threads");
  std::thread dog([] { ArmThisThread(); if (WaitForSingleObject(doneEvt, 10000) == WAIT_TIMEOUT) { Log("shutdown took over 10 s - forcing exit"); TerminateProcess(GetCurrentProcess(), 2); } }); dog.detach();
  ft.join(); rec.join(); rt.join(); st.join(); qw.join(); wt.join();
  kinectOk = 0; WriteStatus(); if (view) { hdr->magic = 0; UnmapViewOfFile(view); view = nullptr; }
  Log("akfeed stopped cleanly - Kinect released"); if (LF) fclose(LF), LF = nullptr;
  SetEvent(doneEvt); ReleaseMutex(mtx); CloseHandle(mtx); return 0; }
