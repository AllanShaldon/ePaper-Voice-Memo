#include <unity.h>
#include "PonteProtocol.h"

// The bridge's real answer shape (2026-10-09), accents raw UTF-8 as Node sends them.
static const char* kResposta =
    "{\"agora\":1791633600,\"lembretes\":["
    "{\"id\":12,\"texto\":\"Ligar para o contador\",\"quandoTs\":1791637200,\"temHorario\":true,\"concluido\":false,\"origem\":\"dispositivo\"},"
    "{\"id\":7,\"texto\":\"Comprar p\xC3\xA3o \\\"integral\\\"\",\"quandoTs\":0,\"temHorario\":false,\"concluido\":false,\"origem\":\"ponte\"}"
    "],\"aviso\":\"sem_horario\",\"transcricao\":\"me lembra\"}";

void test_parses_list_and_server_clock()
{
  std::vector<PonteItem> v;
  long long agora = 0;
  TEST_ASSERT_TRUE(vmPonteParseLista(kResposta, v, agora));
  TEST_ASSERT_EQUAL(1791633600LL, agora);
  TEST_ASSERT_EQUAL(2, (int)v.size());
  TEST_ASSERT_EQUAL(12LL, v[0].id);
  TEST_ASSERT_EQUAL_STRING("Ligar para o contador", v[0].texto.c_str());
  TEST_ASSERT_EQUAL(1791637200LL, v[0].quandoTs);
  TEST_ASSERT_TRUE(v[0].temHorario);
}

void test_text_escapes_and_utf8_survive()
{
  std::vector<PonteItem> v;
  long long agora = 0;
  vmPonteParseLista(kResposta, v, agora);
  TEST_ASSERT_EQUAL_STRING("Comprar p\xC3\xA3o \"integral\"", v[1].texto.c_str());
  TEST_ASSERT_FALSE(v[1].temHorario);
}

void test_unicode_escape_becomes_utf8()
{
  std::vector<PonteItem> v;
  long long agora = 0;
  TEST_ASSERT_TRUE(vmPonteParseLista(
      "{\"lembretes\":[{\"id\":1,\"texto\":\"Reuni\\u00e3o \\ud83d\\ude00\",\"quandoTs\":5}]}", v, agora));
  TEST_ASSERT_EQUAL_STRING("Reuni\xC3\xA3o \xF0\x9F\x98\x80", v[0].texto.c_str());
}

void test_empty_list_is_valid()
{
  std::vector<PonteItem> v(1);
  long long agora = 0;
  TEST_ASSERT_TRUE(vmPonteParseLista("{\"agora\":1,\"lembretes\":[]}", v, agora));
  TEST_ASSERT_EQUAL(0, (int)v.size());
}

void test_error_body_keeps_local_copy()
{
  // An error page or a truncated body must NOT read as "the list is empty":
  // that would wipe the screen on every network hiccup.
  std::vector<PonteItem> v(3);
  long long agora = 0;
  TEST_ASSERT_FALSE(vmPonteParseLista("{\"erro\":\"token do aparelho ausente ou inválido\"}", v, agora));
  TEST_ASSERT_FALSE(vmPonteParseLista("{\"lembretes\":[{\"id\":1,\"texto\":\"cort", v, agora));
  TEST_ASSERT_FALSE(vmPonteParseLista("<html>502</html>", v, agora));
  TEST_ASSERT_EQUAL(3, (int)v.size());
}

void test_string_field_and_body_escape()
{
  TEST_ASSERT_EQUAL_STRING("sem_horario", vmPonteCampo(kResposta, "aviso").c_str());
  TEST_ASSERT_EQUAL_STRING("", vmPonteCampo(kResposta, "nao_existe").c_str());
  TEST_ASSERT_EQUAL_STRING("\"diz \\\"oi\\\" \\\\ ok\"", vmPonteJsonString("diz \"oi\" \\ ok").c_str());
}

void test_clock_conversion_round_trips()
{
  const long tz = -3 * 3600;
  TEST_ASSERT_EQUAL(1791637200LL - 10800, vmPonteLocalEpoch(1791637200LL, tz));
  TEST_ASSERT_EQUAL(1791637200LL, vmPonteUtcEpoch(vmPonteLocalEpoch(1791637200LL, tz), tz));
  TEST_ASSERT_EQUAL(0LL, vmPonteLocalEpoch(0, tz));   // "no time" stays no time
}

void test_merge_rule()
{
  const long long now = 1000000, ttl = 300;
  TEST_ASSERT_TRUE(vmPonteKeepLocalOnly(0, false, 0, false, now, ttl));           // not uploaded yet
  TEST_ASSERT_TRUE(vmPonteKeepLocalOnly(9, true, now, true, now, ttl));           // our change pending
  TEST_ASSERT_TRUE(vmPonteKeepLocalOnly(9, true, now - 10, false, now, ttl));     // checked off just now
  TEST_ASSERT_FALSE(vmPonteKeepLocalOnly(9, true, now - 400, false, now, ttl));   // window over
  TEST_ASSERT_FALSE(vmPonteKeepLocalOnly(9, false, 0, false, now, ttl));          // finished elsewhere
  TEST_ASSERT_TRUE(vmPonteLocalDoneWins(true));
  TEST_ASSERT_FALSE(vmPonteLocalDoneWins(false));
}

int main(int, char**)
{
  UNITY_BEGIN();
  RUN_TEST(test_parses_list_and_server_clock);
  RUN_TEST(test_text_escapes_and_utf8_survive);
  RUN_TEST(test_unicode_escape_becomes_utf8);
  RUN_TEST(test_empty_list_is_valid);
  RUN_TEST(test_error_body_keeps_local_copy);
  RUN_TEST(test_string_field_and_body_escape);
  RUN_TEST(test_clock_conversion_round_trips);
  RUN_TEST(test_merge_rule);
  return UNITY_END();
}
