
/* Tokenizes a string. The string str is separated into substrings
 * specified by the list of delimiters. The result is put into the
 * vector tokens */
void Tokenize(const std::string& str,
              std::vector<std::string>& tokens,
              const std::string& delimiters = " ");
