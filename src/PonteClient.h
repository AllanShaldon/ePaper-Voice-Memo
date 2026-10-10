// PonteClient.h -- talks to the bridge (ponte) on the home server, the ONE
// store of reminders shared with the WhatsApp bridge (2026-10-09).
//
//   enviarVoz  POST /api/dispositivo/voz       WAV -> transcribe + what/when -> new reminder
//   listar     GET  /api/dispositivo/lembretes  the active list (what fits on screen)
//   concluir   POST /api/dispositivo/lembretes/<id>/concluir {"concluido":bool}
//   importar   POST /api/dispositivo/lembretes  {"texto","quandoTs"} (local-only reminder)
//
// Every call answers with the full list, so one round trip both acts and syncs.
// Auth is the device's OWN token (Authorization: Bearer), never the bridge's
// master token: this box sits on a desk.

#ifndef VOICE_MEMO_PONTE_CLIENT_H
#define VOICE_MEMO_PONTE_CLIENT_H

#include <Arduino.h>
#include <vector>

#include "PonteProtocol.h"

struct PonteConfig {
  const char* url;     // base, e.g. https://host.example  (no trailing slash)
  const char* token;   // device token from POST /api/config/dispositivo-token
};

struct PonteResposta {
  int httpCode = 0;
  bool listaOk = false;            // `lista` came back and parsed
  std::vector<PonteItem> lista;
  long long agoraUtc = 0;
  long long criadoId = 0;          // enviarVoz / importar: id of the new reminder
  String aviso;                    // enviarVoz: "", "sem_horario", "ia_indisponivel"
  String transcricao;
};

class PonteClient {
 public:
  void configure(const PonteConfig& cfg, uint32_t httpTimeoutMs);
  bool ativa() const;   // url + token present

  bool enviarVoz(const uint8_t* wav, size_t bytes, PonteResposta& out);
  bool listar(PonteResposta& out);
  bool concluir(long long id, bool concluido, PonteResposta& out);
  bool importar(const String& texto, long long quandoTsUtc, PonteResposta& out);

 private:
  bool requisitar(const char* metodo, const String& caminho, const char* contentType,
                  const uint8_t* corpo, size_t bytes, PonteResposta& out);
  String   url_;
  String   token_;
  uint32_t timeoutMs_ = 30000;
};

#endif  // VOICE_MEMO_PONTE_CLIENT_H
