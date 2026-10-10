#include "PonteClient.h"

#include <HTTPClient.h>
#include <WiFi.h>
#include <WiFiClientSecure.h>

void PonteClient::configure(const PonteConfig& cfg, uint32_t httpTimeoutMs)
{
  url_ = cfg.url ? cfg.url : "";
  while (url_.endsWith("/")) url_.remove(url_.length() - 1);
  token_ = cfg.token ? cfg.token : "";
  timeoutMs_ = httpTimeoutMs;
}

bool PonteClient::ativa() const
{
  return url_.length() > 0 && token_.length() >= 24;
}

bool PonteClient::requisitar(const char* metodo, const String& caminho, const char* contentType,
                             const uint8_t* corpo, size_t bytes, PonteResposta& out)
{
  out = PonteResposta();
  if (!ativa() || WiFi.status() != WL_CONNECTED) return false;

  HTTPClient http;
  WiFiClientSecure secure;
  const String alvo = url_ + caminho;
  bool aberto;
  if (alvo.startsWith("https://")) {
    // Same choice as the other clients in this firmware (no CA bundle on
    // board); the request still carries a per-device token that only opens
    // the device routes, and the bridge can rotate it at any time.
    secure.setInsecure();
    aberto = http.begin(secure, alvo);
  } else {
    aberto = http.begin(alvo);
  }
  if (!aberto) return false;
  http.setTimeout(timeoutMs_);
  http.addHeader("Authorization", String("Bearer ") + token_);
  if (contentType) http.addHeader("Content-Type", contentType);

  int code;
  if (strcmp(metodo, "GET") == 0) {
    code = http.GET();
  } else {
    code = http.sendRequest(metodo, const_cast<uint8_t*>(corpo), bytes);
  }
  const String body = http.getString();
  http.end();
  out.httpCode = code;
  Serial1.printf("[ponte] %s %s -> %d (%u bytes)\n", metodo, caminho.c_str(), code,
                 static_cast<unsigned>(body.length()));

  const std::string json(body.c_str(), body.length());
  out.listaOk = vmPonteParseLista(json, out.lista, out.agoraUtc);
  out.aviso = vmPonteCampo(json, "aviso").c_str();
  out.transcricao = vmPonteCampo(json, "transcricao").c_str();
  // "criado":{"id":N,...}: the first "id" after the "criado" key.
  const int c = body.indexOf("\"criado\":{");
  if (c >= 0) {
    const int i = body.indexOf("\"id\":", c);
    if (i >= 0) out.criadoId = atoll(body.c_str() + i + 5);
  }
  return code >= 200 && code < 300;
}

bool PonteClient::enviarVoz(const uint8_t* wav, size_t bytes, PonteResposta& out)
{
  return requisitar("POST", "/api/dispositivo/voz", "audio/wav", wav, bytes, out);
}

bool PonteClient::listar(PonteResposta& out)
{
  return requisitar("GET", "/api/dispositivo/lembretes", nullptr, nullptr, 0, out);
}

bool PonteClient::concluir(long long id, bool concluido, PonteResposta& out)
{
  const String corpo = concluido ? "{\"concluido\":true}" : "{\"concluido\":false}";
  return requisitar("POST", "/api/dispositivo/lembretes/" + String(static_cast<long>(id)) + "/concluir",
                    "application/json", reinterpret_cast<const uint8_t*>(corpo.c_str()), corpo.length(), out);
}

bool PonteClient::importar(const String& texto, long long quandoTsUtc, PonteResposta& out)
{
  const std::string t(texto.c_str(), texto.length());
  const String corpo = String("{\"texto\":") + vmPonteJsonString(t).c_str() +
                       ",\"quandoTs\":" + String(static_cast<long>(quandoTsUtc)) + "}";
  return requisitar("POST", "/api/dispositivo/lembretes", "application/json",
                    reinterpret_cast<const uint8_t*>(corpo.c_str()), corpo.length(), out);
}
