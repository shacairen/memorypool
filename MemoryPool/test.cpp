#include"Allocate.h"
#include<string>

using std::string;

class MyClass {
private:
	int num;
	char c[30];
public:
	MyClass() { cout << "gouzao" << endl; }
	void Write(string s);
	string Read();
	~MyClass() { cout << "xigou" << endl; }
	int ReadNum() { return num; }
};

void MyClass::Write(string s) {
	strcpy(c, s.c_str());
}

string MyClass::Read() {
	string str = c;
	return str;
}



int main() {
	
	MyClass *m = New<MyClass>();
	cout << sizeof(*m) << endl;
	//²âÊÔ´úÂë 
	//m->Write("test");
	//cout << m->Read() << endl;
	cout << m->ReadNum() << endl;
	//
	Delete<MyClass>(m);
	return 0;
}