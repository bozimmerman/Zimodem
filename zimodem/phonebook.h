/*
   Copyright 2016-2024 Bo Zimmerman

   Licensed under the Apache License, Version 2.0 (the "License");
   you may not use this file except in compliance with the License.
   You may obtain a copy of the License at

     http://www.apache.org/licenses/LICENSE-2.0

   Unless required by applicable law or agreed to in writing, software
   distributed under the License is distributed on an "AS IS" BASIS,
   WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
   See the License for the specific language governing permissions and
   limitations under the License.
*/
class PhoneBookEntry
{
  public:
    unsigned long number;
    const char *address;
    const char *modifiers;
    const char *notes;
    PhoneBookEntry *next = null;

    PhoneBookEntry(unsigned long phnum, const char *addr, const char *mod, const char *note);
    ~PhoneBookEntry();

    static void loadPhonebook();
    static void clearPhonebook();
    static void savePhonebook();
    static bool checkPhonebookEntry(String cmd);
    static PhoneBookEntry *findPhonebookEntry(long number);
    static PhoneBookEntry *findPhonebookEntry(String number);
};

