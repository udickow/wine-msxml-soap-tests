/* -*- Mode: C; c-file-style: "stroustrup"; indent-tabs-mode: nil -*- */
/*
 * XML test
 *
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

/* Parts of this file come from Wine's dlls/msxml3/tests/domdoc.c */

/* This program tests a few fixed cases of defining namespaces. */

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

#define RELEASE_ELEMENT(e) \
    do { if (e != NULL) IXMLDOMElement_Release(e); } while(0)

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

/* Helper macro to log the important calls, including interesting arguments, no matter
 * the return status, but returning from the current function if HRESULT hr is not ok.
 */
#define CHK_HR(fmt,args...) \
    do { printf("%s <-- " fmt , (hr == S_OK ? "ok  " : "FAIL") , ##args); \
         if (hr != S_OK) goto CleanReturn; \
    } while(0)

/* Simple, easy way of setting any attribute, including a namespace binding */
static HRESULT set_attr_easy(IXMLDOMElement *elem, const char *attr, const char *str_val)
{
    HRESULT hr;
    VARIANT var;

    V_VT(&var) = VT_BSTR;
    V_BSTR(&var) = _bstr_(str_val);

    hr = IXMLDOMElement_setAttribute(elem, _bstr_(attr), var);
    CHK_HR("  setAttribute (attr = \"%s\", value = \"%s\"\n", attr, str_val);

CleanReturn:
    return hr;
}

/* Convoluted way of setting attributes, fails for the bare xmlns in wine <= 1.5.4,
 * but succeeds with the native msxml3 from winetricks (msxml3.msi Service Pack 7).
 * This silly way of doing it has been reconstructed from a trace of BridgeCentral
 * run with WINEDEBUG=msxml and then trying to enter a "DBf kode" (trigger SOAP).
 *
 * We first create a detached attribute node (type 2) with explicit namespace setting
 * (that Wine may well continue to ignore for the time being -- not wanted in output),
 * then set the value field of the node,
 * finally apply the attribute node to the given element node.
 * Actually we begin by crawling back from element to doc (trace does that too).
 */
static HRESULT set_attr_cplx(IXMLDOMElement *elem, const char *attr, const char *str_val)
{
    /* The following namespace URI is used at initial creation of the reserved
     * attributes "xmlns:..." and "xmlns" by both bug 26226 apps as seen in traces,
     * and also occurs in 'strings -e l BridgeCentral.exe'.  Note that W3C says in
     * http://www.w3.org/TR/xml-names/#ns-decl that "xlmns" is already by definition
     * bound to this URI and MUST NOT be declared.  (OTOH Microsoft's
     * http://msdn.microsoft.com/en-us/library/windows/desktop/ms757901%28v=vs.85%29.aspx
     * says that prefixed names MUST have a non-empty nsURI in the createNode call).
     * So Wine should continue to filter out this URI (for attribute xmlns[:...]) even
     * if it some day begins to support namespaced attribute nodes properly.
     */
    const char *nsURI = "http://www.w3.org/2000/xmlns/";
    HRESULT hr;
    VARIANT var;
    IXMLDOMDocument *doc;
    IXMLDOMAttribute *attr_node, *attr_old;

    /* 0) Find doc from given element */
    hr = IXMLDOMElement_get_ownerDocument(elem, &doc);
    if (hr != S_OK)
    {   /* This error should never happen. */
        printf("set_attr_cplx: failed to find doc from elem\n");
        goto CleanReturn;
    }

    /* 1) Create attribute node */
    V_VT(&var) = VT_I4;  // VT_I1 normally, but I4 seen in trace, so use that now
    V_I4(&var) = NODE_ATTRIBUTE;

    hr = IXMLDOMDocument_createNode(doc, var, _bstr_(attr),
                                    _bstr_(nsURI), (IXMLDOMNode**)&attr_node);

    CHK_HR("  createNode (type = NODE_ATTRIBUTE, attr = \"%s\", nsURI = \"%s\")\n",
           attr, nsURI);

    /* 2) Put attribute value into attribute node */
    V_VT(&var) = VT_BSTR;
    V_BSTR(&var) = _bstr_(str_val);

    hr = IXMLDOMAttribute_put_nodeValue(attr_node, var);
    CHK_HR("    put_nodeValue (value = \"%s\")\n", str_val);

    /* 3) Connect/transfer our new attribute node to the given element node */
    hr = IXMLDOMElement_setAttributeNode(elem, attr_node, &attr_old);
    CHK_HR("    setAttributeNode\n");

CleanReturn:
    return hr;
}

static HRESULT set_attr(IXMLDOMElement *elem, const char *attr, const char *str_val,
                        BOOL use_node)
{
    return (use_node ? set_attr_cplx(elem, attr, str_val)
                     : set_attr_easy(elem, attr, str_val));
}

/* Create an element via createNode directly, using supplied nsURI for the namespace URI.
 * nsURI may be the empty string ("" used by createElement according to MSDN docs).
 * NULL is used by createElement in Wine currently and thus not interesting to test here;
 * in fact we forbid it.
 * The body is mostly a copy of domdoc_createElement, minus some error checking & debug.
 */
static IXMLDOMElement* create_elem_ns(IXMLDOMDocument *doc, const char *name,
                                      const char *nsURI)
{
    VARIANT type;
    HRESULT hr;
    IXMLDOMNode *node;
    IXMLDOMElement* element = NULL;

    assert(nsURI != NULL);

    V_VT(&type) = VT_I1;
    V_I1(&type) = NODE_ELEMENT;

    hr = IXMLDOMDocument_createNode(doc, type, _bstr_(name), _bstr_(nsURI), &node);
    CHK_HR("createNode (type = NODE_ELEMENT, name = \"%s\", nsURI = \"%s\")\n", name, nsURI);

    IXMLDOMNode_QueryInterface(node, &IID_IXMLDOMElement, (void**) &element);
    IXMLDOMNode_Release(node);

CleanReturn:
    return element;
}


static IXMLDOMDocument* create_doc(void)
{
    HRESULT hr;
    IXMLDOMDocument *doc = NULL;

    hr = CoCreateInstance( &CLSID_DOMDocument, NULL, CLSCTX_INPROC_SERVER,
                           &IID_IXMLDOMDocument, (void**)&doc );
    CHK_HR("CoCreateInstance (...DOMDocument...)\n");

    /* First set attributes like BridgeCentral would do in its request */
    IXMLDOMDocument_put_preserveWhiteSpace(doc, VARIANT_FALSE);
    IXMLDOMDocument_put_resolveExternals(doc, VARIANT_FALSE);
    IXMLDOMDocument_put_validateOnParse(doc, VARIANT_FALSE);
    IXMLDOMDocument_put_async(doc, VARIANT_FALSE);

CleanReturn:
    return doc;
}


static void print_xml(const char *fmt, IXMLDOMElement *elem)
{
    HRESULT hr;
    BSTR xml;

    hr = IXMLDOMElement_get_xml(elem, &xml);
    if(hr == S_OK)
    {
        printf(fmt, wine_dbgstr_w(xml));
    }
    else
        printf("Getting back the XML failed\n");
    SysFreeString(xml);
}


static void test_xmlns(void)
{
    HRESULT hr;
    IXMLDOMElement *elem1, *elem2;

    IXMLDOMDocument *doc = create_doc();
    if (doc == NULL) return;

    /* Test inheritance and possible duplication of default namespace */

    elem1 = create_elem_ns(doc, "elem1", "urn:ns1");
    elem2 = create_elem_ns(doc, "elem2", "urn:ns1");

    hr = IXMLDOMElement_appendChild(elem1, (IXMLDOMNode*)elem2, NULL);
    CHK_HR("appendChild (elem1, elem2, NULL)\n");

    print_xml("dbgstr(elem1) = %s\n", elem1);

CleanReturn:
    RELEASE_ELEMENT(elem1);
    RELEASE_ELEMENT(elem2);
    free_bstrs();
    IXMLDOMDocument_Release(doc);
}


int main(void)
{
    HRESULT hr;

    hr = CoInitialize( NULL );

    if (hr == S_OK)
        printf("CoInitialize successful!\n");
    else
    {
        printf("Failed to init com\n");
        return 1;
    }

    test_xmlns();

    CoUninitialize();
    return 0;
}
