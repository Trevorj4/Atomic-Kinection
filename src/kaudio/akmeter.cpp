// prints the level on "CABLE Output" every 10 s for N seconds
#include <windows.h>
#include <mmdeviceapi.h>
#include <audioclient.h>
#include <functiondiscoverykeys_devpkey.h>
#include <stdio.h>
#include <math.h>
int main(int argc, char** argv) { int secs = argc > 1 ? atoi(argv[1]) : 120; CoInitializeEx(0, COINIT_MULTITHREADED);
  IMMDeviceEnumerator* e; CoCreateInstance(__uuidof(MMDeviceEnumerator), 0, CLSCTX_ALL, IID_PPV_ARGS(&e)); IMMDeviceCollection* col; e->EnumAudioEndpoints(eCapture, DEVICE_STATE_ACTIVE, &col);
  UINT n; col->GetCount(&n); IMMDevice* d = 0; for (UINT i = 0; i < n && !d; i++) { IMMDevice* x; col->Item(i, &x); IPropertyStore* ps; x->OpenPropertyStore(STGM_READ, &ps); PROPVARIANT v; PropVariantInit(&v); ps->GetValue(PKEY_Device_FriendlyName, &v); if (v.pwszVal && wcsstr(v.pwszVal, L"CABLE Output")) d = x; }
  if (!d) { printf("no CABLE Output\n"); return 1; }
  IAudioClient* ac; d->Activate(__uuidof(IAudioClient), CLSCTX_ALL, 0, (void**)&ac); WAVEFORMATEX* mf; ac->GetMixFormat(&mf); ac->Initialize(AUDCLNT_SHAREMODE_SHARED, 0, 400000, 0, mf, 0);
  IAudioCaptureClient* cc; ac->GetService(IID_PPV_ARGS(&cc)); ac->Start(); double sq = 0, tsq = 0; long long cnt = 0, tc = 0, silentPk = 0, pk = 0; DWORD t0 = GetTickCount(), lt = t0;
  while (GetTickCount() - t0 < (DWORD)secs * 1000) { Sleep(20); UINT32 k; while (SUCCEEDED(cc->GetNextPacketSize(&k)) && k) { BYTE* p; DWORD fl; cc->GetBuffer(&p, &k, &fl, 0, 0); pk++;
      if (fl & AUDCLNT_BUFFERFLAGS_SILENT) silentPk++; else { float* f = (float*)p; for (UINT32 i = 0; i < k * mf->nChannels; i++) sq += f[i] * f[i]; } cnt += k * mf->nChannels; cc->ReleaseBuffer(k); }
    if (GetTickCount() - lt >= 10000) { printf("CABLE Output %.1f dBFS\n", 10 * log10(sq / (cnt ? cnt : 1) + 1e-20)); fflush(stdout); tsq += sq; tc += cnt; sq = 0; cnt = 0; lt = GetTickCount(); } }
  tsq += sq; tc += cnt; printf("RESULT CABLE Output avg %.1f dBFS packets %lld silent %lld\n", 10 * log10(tsq / (tc ? tc : 1) + 1e-20), pk, silentPk); return 0; }
