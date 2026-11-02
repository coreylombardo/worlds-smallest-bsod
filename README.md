what am i doing with my life

(C89)

# breakdown

full expanded form:

```C
#include <windows.h>

typedef LONG NTSTATUS;
typedef NTSTATUS (NTAPI *pRtlSetProcessIsCritical)(
    BOOLEAN bNew,
    BOOLEAN *pbOld,
    BOOLEAN bNeedScm
);

BOOL EnablePrivilege(LPCWSTR priv) {
    HANDLE token;
    TOKEN_PRIVILEGES tp;

    if (!OpenProcessToken(GetCurrentProcess(), TOKEN_ADJUST_PRIVILEGES | TOKEN_QUERY, &token))
        return FALSE;
    LookupPrivilegeValueW(NULL, priv, &tp.Privileges[0].Luid);
    tp.PrivilegeCount = 1;
    tp.Privileges[0].Attributes = SE_PRIVILEGE_ENABLED;
    AdjustTokenPrivileges(token, FALSE, &tp, 0, NULL, NULL);
    CloseHandle(token);
    return TRUE;
}

int main(void) {
    HMODULE hNtdll = GetModuleHandleW(L"ntdll.dll");

    pRtlSetProcessIsCritical RtlSetProcessIsCritical =
        (pRtlSetProcessIsCritical)GetProcAddress(hNtdll, "RtlSetProcessIsCritical");

    EnablePrivilege(L"SeDebugPrivilege");

    BOOLEAN oldVal = FALSE;
    NTSTATUS status = RtlSetProcessIsCritical(TRUE, &oldVal, FALSE);

    ExitProcess(1);
}
```

first, remove everything the easy stuff:
- ```ExitProcess```, for obvious reasons
- ```CloseHandle```, we don't need handle cleanup because the machine is about to BSOD
- we never check ```status``` either, so that can go
  
```C
#include <windows.h>

typedef LONG NTSTATUS;
typedef NTSTATUS (NTAPI *pRtlSetProcessIsCritical)(
    BOOLEAN bNew,
    BOOLEAN *pbOld,
    BOOLEAN bNeedScm
);

BOOL EnablePrivilege(LPCWSTR priv) {
    HANDLE token;
    TOKEN_PRIVILEGES tp;

    OpenProcessToken(GetCurrentProcess(), TOKEN_ADJUST_PRIVILEGES | TOKEN_QUERY, &token);
    LookupPrivilegeValueW(NULL, priv, &tp.Privileges[0].Luid);
    tp.PrivilegeCount = 1;
    tp.Privileges[0].Attributes = SE_PRIVILEGE_ENABLED;
    AdjustTokenPrivileges(token, FALSE, &tp, 0, NULL, NULL);
}

int main(void) {
    HMODULE hNtdll = GetModuleHandleW(L"ntdll.dll");

    pRtlSetProcessIsCritical RtlSetProcessIsCritical =
        (pRtlSetProcessIsCritical)GetProcAddress(hNtdll, "RtlSetProcessIsCritical");

    EnablePrivilege(L"SeDebugPrivilege");

    BOOLEAN oldVal = FALSE;
    RtlSetProcessIsCritical(TRUE, &oldVal, FALSE);
}
```

the following aren't *explicitly* required:
- ```RtlAdjustPrivilege``` does the following three internally:
  - ```OpenProcessToken```
  - ```LookupPrivilegeValueW```
    - this line just exists to translate the string into a {20, 0} LUID, which is fixed in the kernel across every windows version AFAIK, so just write 20 straight up with ```RtlAdjustPrivilege``` and skip the lookup
  - ```AdjustTokenPrivileges```
- ```EnablePrivilege```
- shorten some variable names and remove unnecessary ones

```C
#include <windows.h>

typedef LONG NTSTATUS;
typedef NTSTATUS (NTAPI *pRtlAdjustPrivilege)(
    ULONG,
    BOOLEAN,
    BOOLEAN,
    PBOOLEAN
);
typedef NTSTATUS (NTAPI *pRtlSetProcessIsCritical)(
    BOOLEAN,
    BOOLEAN*,
    BOOLEAN
);

int main(void) {
    HMODULE hNtdll = GetModuleHandleW(L"ntdll.dll");

    pRtlAdjustPrivilege RtlAdjustPrivilege =
        (pRtlAdjustPrivilege)GetProcAddress(hNtdll, "RtlAdjustPrivilege");
    pRtlSetProcessIsCritical RtlSetProcessIsCritical =
        (pRtlSetProcessIsCritical)GetProcAddress(hNtdll, "RtlSetProcessIsCritical");

    BOOLEAN old = FALSE;
    RtlAdjustPrivilege(20, TRUE, FALSE, &old);
    RtlSetProcessIsCritical(TRUE, &old, FALSE);
}
```

- ```GetModuleHandleW(L"ntdll.dll");``` -> ```LoadLibraryA("ntdll")``` to save some space
  - no L prefix and no ".dll", windows actually appends this automatically
- inline casting shenanigans
  - we can delete all the ```typedef```s and replace them with inline casts at the call site
  - delete pointer variables: ```GetProcAddress``` result cast and called in one line

```C
#include <windows.h>

int main(void) {
    void *n = LoadLibraryA("ntdll");
    int old;

    ((void(*)(int,int,int,int*))GetProcAddress(n, "RtlAdjustPrivilege"))(20, 1, 0, &old);
    ((void(*)(int,int,int))GetProcAddress(n, "RtlSetProcessIsCritical"))(1, 0, 0);
}
```

- the astute amongst you may notice we have a repeating ```GetProcAddress(n, ...)``` pattern here. MACRO IT
  - no more ```n```, meaning ```LoadLibraryA("ntdll")``` technically gets called twice, but ntdll is always already loaded
- the pointer casts (```(void(*)(int,int,int,int*)``` etc) are pretty clunky too
  - ```GetProcAddress``` returns ```FARPROC``` which, in C11, has ```()``` (unspecified parameters), so we can call it with *any* arguments *without* a cast
- change ```int main(void)``` to ```int main(int x)```
  - ```RtlAdjustPrivilege``` needs somewhere to write its output, **it does not matter where**
  - ```x``` is actually just renamed ```argc```, which apparently is a valid writable ```int``` on the stack. cool!
  - ```old``` is redundant now

```C
#include <windows.h>
#define G(f) (GetProcAddress(LoadLibraryA("ntdll"), f))

int main(int x) {
    G("RtlAdjustPrivilege")(20, 1, 0, &x);
    G("RtlSetProcessIsCritical")(1, 0, 0);
}
```

now we can use funny C89 stuff to kill all the type keywords:
- ```int main(int x)``` -> ```x; main()```
  - in C89, the return type of functions and type of global variables is ```int``` if not specified
    - ```int x``` is moved outside the main function and becomes global
   
```C
#include <windows.h>
#define G(f)(GetProcAddress(LoadLibraryA("ntdll"),f))

x;
main() {
    G("RtlAdjustPrivilege")(20, 1, 0, &x);
    G("RtlSetProcessIsCritical")(1, 0, 0);
}
```

- remove stupid linebreaks and spaces in arguments
  - yes, even ```#include <windows.h>```, that's not enforced either!
 
```C
#include<windows.h>
#define G(f)(GetProcAddress(LoadLibraryA("ntdll"),f))
x;main(){G("RtlAdjustPrivilege")(20,1,0,&x);G("RtlSetProcessIsCritical")(1,0,0);}
```
