#include "PonteClient.h"

#include <HTTPClient.h>
#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <esp_netif.h>

// Public DNS for the bridge host. Measured 2026-10-10: the home router (Vivo)
// answered NXDOMAIN for the bridge's name from its cache while 1.1.1.1/8.8.8.8
// resolved it fine -- the device just saw "-1, connection failed" and every
// reminder said "server unreachable". Set VM_PONTE_DNS1 to "" to keep the
// router's DNS.
#ifndef VM_PONTE_DNS1
#define VM_PONTE_DNS1 "1.1.1.1"
#endif
#ifndef VM_PONTE_DNS2
#define VM_PONTE_DNS2 "8.8.8.8"
#endif

namespace {

void definirDns(esp_netif_t* n, esp_netif_dns_type_t tipo, const char* ip)
{
  if (!ip || !ip[0]) return;
  esp_netif_dns_info_t d = {};
  d.ip.type = ESP_IPADDR_TYPE_V4;
  d.ip.u_addr.ip4.addr = esp_ip4addr_aton(ip);
  esp_netif_set_dns_info(n, tipo, &d);
}

// Re-applied before every call: a DHCP renewal hands the router's DNS back.
void fixarDnsPublico()
{
  esp_netif_t* n = esp_netif_get_handle_from_ifkey("WIFI_STA_DEF");
  if (!n) return;
  definirDns(n, ESP_NETIF_DNS_MAIN, VM_PONTE_DNS1);
  definirDns(n, ESP_NETIF_DNS_BACKUP, VM_PONTE_DNS2);
}

// "https://host[:port]/..." -> "host"
String hostDaUrl(const String& url)
{
  int i = url.indexOf("://");
  i = (i < 0) ? 0 : i + 3;
  int fim = url.length();
  const int barra = url.indexOf('/', i);
  const int doisPontos = url.indexOf(':', i);
  if (barra >= 0) fim = barra;
  if (doisPontos >= 0 && doisPontos < fim) fim = doisPontos;
  return url.substring(i, fim);
}

}  // namespace

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

  fixarDnsPublico();
  // Name first, so a DNS failure is logged as one and not as a bare "-1".
  const String host = hostDaUrl(url_);
  IPAddress ip;
  if (!WiFi.hostByName(host.c_str(), ip)) {
    Serial1.printf("[ponte] DNS nao resolveu %s -- nada enviado\n", host.c_str());
    out.httpCode = -100;
    return false;
  }

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
