/* -*- Mode: C; c-file-style: "stroustrup"; indent-tabs-mode: nil -*- */
/*
 * XML test
 *
 * Copyright 2005 Mike McCormack for CodeWeavers
 * Copyright 2007-2008 Alistair Leslie-Hughes
 * Copyright 2010-2011 Adam Martinson for CodeWeavers
 * Copyright 2010-2012 Nikolay Sivov for CodeWeavers
 * Copyright 2012 Ulrik Dickow
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301, USA
 */

#define COBJMACROS
#define CONST_VTABLE

#include <stdio.h>
#include <assert.h>

#include "windows.h"

#include "msxml2.h"
#include "msxml2did.h"
#include "ole2.h"
#include "dispex.h"

/* undef the #define in msxml2 so that it compiles stand-alone with -luuid */
#undef CLSID_DOMDocument

/* From Wine source wine/test.h: */
extern const char *wine_dbgstr_wn( const WCHAR *str, int n );
static inline const char *wine_dbgstr_w( const WCHAR *s ) { return wine_dbgstr_wn( s, -1 ); }



/* Parts of this file come from Wine's dlls/msxml3/tests/domdoc.c */

/*** BSTR helper functions from tests/domdoc.c ***/

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



/* Try building a SOAP request step-by-step like done by BridgeCentral or
 * http://blogs.msdn.com/b/jpsanders/archive/2007/06/14/how-to-send-soap-call-using-msxml-replace-stk.aspx */
void test_build_soap(IXMLDOMDocument *doc)
{
    HRESULT hr;
    IXMLDOMProcessingInstruction *nodePI = NULL;
    // IXMLDOMElement *element;
    BSTR xml;

    /* First set attributes like BridgeCentral would do in its request */
    IXMLDOMDocument_put_preserveWhiteSpace(doc, VARIANT_FALSE);
    IXMLDOMDocument_put_resolveExternals(doc, VARIANT_FALSE);
    IXMLDOMDocument_put_validateOnParse(doc, VARIANT_FALSE);
    IXMLDOMDocument_put_async(doc, VARIANT_FALSE);

    hr = IXMLDOMDocument_createProcessingInstruction(doc, _bstr_("xml"),
                                                     _bstr_("version=\"1.0\""), &nodePI);
    if(hr != S_OK || nodePI == NULL)
    {
        printf("createProcessingInstruction failed (returns %08x)\n", hr);
        return;
    }
    hr = IXMLDOMDocument_appendChild(doc, (IXMLDOMNode*)nodePI, NULL);
    if(hr != S_OK)
        printf("appending processing instruction as child to doc failed\n");
    IXMLDOMProcessingInstruction_Release(nodePI);

    hr = IXMLDOMDocument_get_xml(doc, &xml);
    if(hr == S_OK)
        printf("Got back this XML (shown as a dbgstr):\n%s\n", wine_dbgstr_w(xml));
    else
        printf("Getting back the XML failed\n");
    SysFreeString(xml);

}

int main(void)
{
    IXMLDOMDocument *doc;
    HRESULT hr;

    hr = CoInitialize( NULL );

    if (hr == S_OK)
        printf("CoInitialize successful!\n");
    else
    {
        printf("Failed to init com\n");
        return 1;
    }

    hr = CoCreateInstance( &CLSID_DOMDocument, NULL, CLSCTX_INPROC_SERVER, &IID_IXMLDOMDocument, (void**)&doc );
    if (hr != S_OK)
    {
        printf("IXMLDOMDocument is not available (0x%08x)\n", hr);

        CoUninitialize();
        return 1;
    }
    printf("DOMDocument succesfully created\n");

    test_build_soap(doc);

    free_bstrs();
    IXMLDOMDocument_Release(doc);
    CoUninitialize();
    return 0;
}
