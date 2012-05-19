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

/* Build with: winegcc -m32 ... -lole32 -loleaut32 -luuid */

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

/***** Begin BSTR helper functions from tests/domdoc.c ***********************/

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

/***** End BSTR helper functions from tests/domdoc.c *************************/

/* Simple, easy way of setting any attribute, including a namespace binding */
static void set_attr_how1(IXMLDOMElement *elem, const char *attr, const char *str_val)
{
    HRESULT hr;
    VARIANT var;

    V_VT(&var) = VT_BSTR;
    V_BSTR(&var) = _bstr_(str_val);

    hr = IXMLDOMElement_setAttribute(elem, _bstr_(attr), var);
    if(hr != S_OK) printf("setting attribute %s to value %s failed\n", attr, str_val);
}

/* Convoluted way of setting attributes, fails for xmlns ones in wine <= 1.5.4 */
static void set_attr_how2(IXMLDOMElement *elem, const char *attr, const char *str_val)
{
    HRESULT hr;
    VARIANT var;

    V_VT(&var) = VT_BSTR;
    V_BSTR(&var) = _bstr_(str_val);

    hr = IXMLDOMElement_setAttribute(elem, _bstr_(attr), var);
    if(hr != S_OK) printf("setting attribute %s to value %s failed\n", attr, str_val);
}

static void set_attr(IXMLDOMElement *elem, const char *attr, const char *str_val, int how)
{
    if(how == 1)
        set_attr_how1(elem, attr, str_val);
    else
        set_attr_how2(elem, attr, str_val);
}


/* Try building a SOAP request step-by-step like in the Visual Basic example
 *    http://blogs.msdn.com/b/jpsanders/archive/2007/06/14/how-to-send-soap-call-using-msxml-replace-stk.aspx
 * to reproduce approximately the SOAP output of BridgeCentral w/ the native dll (winetricks).
 * Wine's msxml3 currently chokes on the calls made by BridgeCentral, spoling its SOAP login.
 * The `how' argument determines how the namespace bindings are made:
 *   how = 1: Set them simply with IXMLDOMElement_setAttribute (like msdn blog example)
 *   how = 2: Set them clumsily via explicit attribute nodes (like BridgeCentral)
 */
static void test_build_soap(IXMLDOMDocument *doc, int how)
{
    HRESULT hr;
    IXMLDOMProcessingInstruction *nodePI = NULL;
    IXMLDOMElement *soapEnvelope, *soapBody, *soapCall, *soapArg1;

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
        free_bstrs();
        return;
    }
    hr = IXMLDOMDocument_appendChild(doc, (IXMLDOMNode*)nodePI, NULL);
    if(hr != S_OK) printf("appending processing instruction as child to doc failed\n");

    IXMLDOMProcessingInstruction_Release(nodePI);

    hr = IXMLDOMDocument_createElement(doc, _bstr_("SOAP-ENV:Envelope"), &soapEnvelope);
    if(hr != S_OK) printf("creation of SOAP envelope element failed\n");

    set_attr(soapEnvelope, "xmlns:SOAP-ENV", "http://schemas.xmlsoap.org/soap/envelope/", how);
    set_attr(soapEnvelope, "xmlns:xsd", "http://www.w3.org/2001/XMLSchema", how);
    set_attr(soapEnvelope, "xmlns:xsi", "http://www.w3.org/2001/XMLSchema-instance", how);

    hr = IXMLDOMDocument_appendChild(doc, (IXMLDOMNode*)soapEnvelope, NULL);
    if(hr != S_OK) printf("appending SOAP envelope as child to doc failed\n");

    hr = IXMLDOMDocument_createElement(doc, _bstr_("SOAP-ENV:Body"), &soapBody);
    if(hr != S_OK) printf("creation of SOAP body element failed\n");

    hr = IXMLDOMElement_appendChild(soapEnvelope, (IXMLDOMNode*)soapBody, NULL);
    if(hr != S_OK) printf("appending SOAP body as child to envelope failed\n");

    hr = IXMLDOMDocument_createElement(doc, _bstr_("Login"), &soapCall);
    if(hr != S_OK) printf("creation of SOAP call element failed\n");

    set_attr(soapCall, "xmlns", "http://www.wso2.org/php/xsd", how);

    hr = IXMLDOMElement_appendChild(soapBody, (IXMLDOMNode*)soapCall, NULL);
    if(hr != S_OK) printf("appending SOAP call as child to body failed\n");

    hr = IXMLDOMDocument_createElement(doc, _bstr_("code"), &soapArg1);
    if(hr != S_OK) printf("creation of SOAP arg1 element failed\n");

    hr = IXMLDOMElement_appendChild(soapCall, (IXMLDOMNode*)soapArg1, NULL);
    if(hr != S_OK) printf("appending SOAP arg1 as child to call failed\n");


    hr = IXMLDOMDocument_get_xml(doc, &xml);
    if(hr == S_OK)
        printf("Got back this XML (shown as a dbgstr):\n%s\n", wine_dbgstr_w(xml));
    else
        printf("Getting back the XML failed\n");
    SysFreeString(xml);

    IXMLDOMElement_Release(soapEnvelope);
    IXMLDOMElement_Release(soapBody);
    free_bstrs();
}

int main(int argc, char **argv)
{
    int how;
    IXMLDOMDocument *doc;
    HRESULT hr;

    if (argc != 2 || ((how = atoi(argv[1])) < 1 || how > 2))
    {
        printf("Usage: %s HOW\n  where HOW is 1 or 2\n", argv[0]);
        return 1;
    }

    hr = CoInitialize( NULL );

    if (hr == S_OK)
        printf("CoInitialize successful!\n");
    else
    {
        printf("Failed to init com\n");
        return 1;
    }

    hr = CoCreateInstance( &CLSID_DOMDocument, NULL, CLSCTX_INPROC_SERVER,
                           &IID_IXMLDOMDocument, (void**)&doc );
    if (hr != S_OK)
    {
        printf("IXMLDOMDocument is not available (0x%08x)\n", hr);

        CoUninitialize();
        return 1;
    }
    printf("DOMDocument succesfully created\n");

    test_build_soap(doc, how);

    IXMLDOMDocument_Release(doc);
    CoUninitialize();
    return 0;
}
