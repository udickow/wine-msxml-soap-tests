#include <stdio.h>
#include <assert.h>

#include "windows.h"
#include "wine/unicode.h"

/* From Wine source wine/test.h (limited to about 300 chars of output, ok for us here): */
extern const char *wine_dbgstr_wn( const WCHAR *str, int n );
static inline const char *wine_dbgstr_w( const WCHAR *s ) { return wine_dbgstr_wn( s, -1 ); }

/***** Begin BSTR helper functions from dlls/msxml3/tests/domdoc.c *********************/
static BSTR alloc_str_from_narrow(const char *str)
{
    int len = MultiByteToWideChar(CP_ACP, 0, str, -1, NULL, 0);
    BSTR ret = SysAllocStringLen(NULL, len - 1);  /* NUL character added automatically */
    MultiByteToWideChar(CP_ACP, 0, str, -1, ret, len);
    return ret;
}

static BSTR alloced_bstrs[256];
static int alloced_bstrs_count;

static BSTR _bstr_(const char *str)
{
    assert(alloced_bstrs_count < sizeof(alloced_bstrs)/sizeof(alloced_bstrs[0]));
    alloced_bstrs[alloced_bstrs_count] = alloc_str_from_narrow(str);
    return alloced_bstrs[alloced_bstrs_count++];
}

static void free_bstrs(void)
{
    int i;
    for (i = 0; i < alloced_bstrs_count; i++)
        SysFreeString(alloced_bstrs[i]);
    alloced_bstrs_count = 0;
}
/***** End BSTR helper functions from dlls/msxml3/tests/domdoc.c ***********************/

void test_pair(const char *nameA, const char *nsURI_A)
{
    static const WCHAR xmlnsW[]  = {'x','m','l','n','s',0};
    static const WCHAR xmlnscW[] = {'x','m','l','n','s',':'};
    static const WCHAR w3xmlns[] = { 'h','t','t','p',':','/','/', 'w','w','w','.','w','3','.',
        'o','r','g','/','2','0','0','0','/','x','m','l','n','s','/',0 };

    BSTR name = _bstr_(nameA);
    BSTR namespaceURI = _bstr_(nsURI_A);

    printf("======== name = \"%s\", nsURI = \"%s\" ============\n", nameA, nsURI_A);

    switch((!strcmpW(name, xmlnsW) ||
            !strncmpW(name, xmlnscW, sizeof(xmlnscW)/sizeof(WCHAR))) +
           !strcmpW(namespaceURI, w3xmlns))
    {
    case 0: /* Neither xmlns nor the reserved W3C xmlns URI */
        printf("attribute nodes with namespaces not yet fully supported.\n");
        break;
    case 1: /* Either xmlns or the reserved URI, but not both -- illegal combination */
        printf("stupid program: prefix xmlns and nsURI %s belong together exclusively.\n",
             wine_dbgstr_w(w3xmlns));
        /* return E_INVALIDARG; */
        break;
    case 2: /* xmlns has been paired with the appropriate URI, so be quiet */
        printf("ok match\n");
        break;
    }
    printf("\n");
    free_bstrs();
}

int main(void)
{
    test_pair("xmlns", "http://www.w3.org/2000/xmlns/"); /* Legal */
    test_pair("xmlns:foo", "http://www.w3.org/2000/xmlns/"); /* Legal */

    test_pair("xmlns", "http://www.w3.org/2000/xmlns"); /* Illegal */
    test_pair("xmlns:foo", "http://www.w3.org/2000/xmlns"); /* Illegal */

    test_pair("xmlns", "http://www.winehq.org/"); /* Illegal */
    test_pair("xmlns:foo", "http://www.winehq.org/"); /* Illegal */

    test_pair("myprefix", "http://www.w3.org/2000/xmlns/"); /* Illegal */
    test_pair("gnus:gnats", "http://www.w3.org/2000/xmlns/"); /* Illegal */

    test_pair("myprefix", "http://www.w3.org/2000/xmlns"); /* Legal, but danger */
    test_pair("gnus:gnats", "http://www.w3.org/2000/xmlns"); /* Legal, but danger */

    return 0;
}
