#include <sstream>
#include <iostream>
#include <string>
int main() {
  std::stringstream ss;
  ss << "BlazePrefab 1\n";
  ss << "rootname Test\n";
  ss << "object\n";
  ss << "name Test\n";
  ss << "type 1\n";
  ss << "pos 10 20 0\n";
  ss << "end\n";
  ss.clear();
  ss.seekg(0);
  std::string line;
  int n=0;
  while (std::getline(ss, line)) { std::cout << n++ << ": [" << line << "]\n"; }
  std::cout << "total=" << n << " str.size=" << ss.str().size() << "\n";
}
