#include "parse_delink.hpp"
#include "test.h"

#include <algorithm>

using namespace std;
using namespace lini;

struct parse_result {
  vector<tuple<string, string, string>> keys;
  vector<int> err;
};
struct delink_result {
  string section, key, value, delinked;
};
struct DelinkSet {
  vector<delink_result> delinked_keys;
  vector<string> delinked_err;
};
using ParseSet = pair<string, parse_result>;

DelinkSet delink_test{
  {
    {"", "key-rogue", "rogue", "rogue"},
    {"test", "key-a", "a", "a"},
    {"test", "ref-ref-a", "${test2.ref-a:failed}", "a"},
    {"test", "ref-self-a", "${key-a:failed}", "a"},
    {"test", "ref-cyclic-1", "${ref-cyclic-2}", ""},
    {"test", "ref-cyclic-2", "${ref-cyclic-1}", ""},
    {"test2", "ref-a", "${test.key-a}", "a"},
    {"test2", "ref-rogue", "${.key-rogue}", "rogue"},
    {"test2", "ref-nexist", "${test.key-nexist: \" f a i l ' }", "\" f a i l '"},
    {"test2", "ref-fallback-a", "${ test.key-a : fail }", "a"},
    {"test2", "ref-fail", "${test.key-fail}", "${test.key-fail}"},
    {"test2", "ref-fake", "{test.key-a}", "{test.key-a}"},
    {"test2", "file-delink", "${file: delink_file.txt }", "content"},
    {"test2", "file-default", "${file:delink_file.txt:fail}", "content"},
    {"test2", "file-nexist", "${file:nexist.txt: \" f a i l ' }", "\" f a i l '"},
    {"test2", "file-fail", "${file:nexist.txt}", "${file:nexist.txt}"},
    {"test2", "env", "${env: test_env: fail}", "test_env"},
    {"test2", "env-nexist", "${env:nexist: \" f a i l \" }", " f a i l "},
    {"test2", "color", "${color: #123456 }", "#123456"},
    {"test2", "color-hsv", "${color: hsv(180, 1, 0.75)}", "#00BFBF"},
    {"test2", "color-ref", "${color: $color}", "#123456"},
    {"test2", "color-mod", "${color: cielch: lum * 1.5, hue + 60; $color}", "#633E5C"},
  }, 
  {
    "test2.ref-fail", "test2.file-fail",
  }
};
ParseSet parse_test = {
  "key-rogue=rogue \n\
[test-excess];excess\n\
[test.forbidden]\n\
[test]\n\
key-a = a \n\
    key b = b\n\
key-cmt = ;cmt\n\
key#forbidden = a\n\
; comment = abc\n\
 key-c=c\n\
key-empty=   \n\
[test-missing\n\
key-test= test \n\
ref-ref-a=${test2.ref-a:failed}\n\
ref-cyclic-1 = ${ref-cyclic-2}\n\
ref-cyclic-2 = ${ref-cyclic-1}\n\
[test2]\n\
key-test2 = test2\n\
key-b='b  '\n\
key-b = 'dup'\n\
key-c = \"  c  \"   \n\
key-c = \"dup\"   \n\
ref-a = ${test.key-a} \n\
ref-rogue= ${.key-rogue} \n\
ref-nexist   = ${test.key-nexist: \" f a i l ' } \n\
ref-fallback-a = ${ test.key-a : failed } \n\
ref-fail   = ${test.key-fail} \n\
ref-fake   = {test.key-a} \n\
key-a = '    a\"",
  {
    { // keys
      {"", "key-rogue", "rogue"},
      {"test", "key-a", "a"},
      {"test", "key-cmt", ";cmt"},
      {"test", "key-c", "c"},
      {"test", "key-empty", ""},
      {"test", "key-test", "test"},
      {"test2", "key-test2", "test2"},
      {"test2", "key-b", "b  "},
      {"test2", "key-c", "  c  "},
      {"test", "ref-ref-a", "${test2.ref-a:failed}"},
      {"test", "ref-cyclic-1", "${ref-cyclic-2}"},
      {"test", "ref-cyclic-2", "${ref-cyclic-1}"},
      {"test2", "ref-a", "${test.key-a}"},
      {"test2", "ref-rogue", "${.key-rogue}"},
      {"test2", "ref-nexist", "${test.key-nexist: \" f a i l ' }"},
      {"test2", "ref-fallback-a", "${ test.key-a : failed }"},
      {"test2", "ref-fail", "${test.key-fail}"},
      {"test2", "ref-fake", "{test.key-a}"},
      {"test2", "key-a", "'    a\""},
    },
    { // err
      2, 3, 6, 8, 12, 20, 22
    },
  }
};

TEST(Parse, general) {
  stringstream ss{parse_test.first};
  document doc;
  errorlist err;
  parse(ss, doc, err);
  auto expected = parse_test.second;
  for(auto& line : expected.err) {
    auto pos = find_if(err.begin(), err.end(), [&](auto it) { return it.first == line; });
    EXPECT_NE(pos, err.end()) << "Expected parsing error: " << line;
    if (pos != err.end())
      err.erase(pos);
  }
  for(auto& e : err) {
    EXPECT_FALSE(true)
      << "Excess parsing error, line num: " << e.first << endl
      << "Message: " << e.second;
  }
  vector<string> found;
  for(auto& key : expected.keys) {
    auto& section = doc[get<0>(key)];
    auto fullkey = get<0>(key) + "." + get<1>(key);
    EXPECT_TRUE(section.find(get<1>(key)) != section.end())
        << "Parse, find: Key: " << fullkey << endl;
    EXPECT_EQ(section[get<1>(key)], get<2>(key))
        << "Parse, compare: Key: " << fullkey << endl;
    found.emplace_back(fullkey);
  }
  for(auto& section : doc) {
    for(auto& keyval : section.second) {
      auto fullkey = section.first + "." + keyval.first;
      EXPECT_NE(std::find(found.begin(), found.end(), fullkey), found.end())
          << "Parse, excess key: " << fullkey << std::endl
          << "Value: " << keyval.second << endl;
    }
  }
}

TEST(Delink, general) {
  str_errlist str_err;
  setenv("test_env", "test_env", true);
  unsetenv("nexist");
  document doc;
  for(auto& tup : delink_test.delinked_keys) {
    doc[tup.section][tup.key] = tup.value;
  }
  delink(doc, str_err);
  for(auto& line : delink_test.delinked_err) {
    auto pos = find_if(str_err.begin(), str_err.end(), [&](auto it) { return it.first == line; });
    EXPECT_NE(pos, str_err.end()) << "Expected delinking error: " << line;
    if (pos != str_err.end())
      str_err.erase(pos);
  }
  for(auto& e : str_err) {
    EXPECT_FALSE(true)
      << "Excess delinking error, key: " << e.first << endl
      << "Message: " << e.second;
  }
  for(auto& tup : delink_test.delinked_keys) {
    EXPECT_EQ(tup.delinked, doc[tup.section][tup.key])
      << "Key: " << tup.section << '.' << tup.key << endl;
  }
}
