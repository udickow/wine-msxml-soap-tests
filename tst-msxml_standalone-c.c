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

/* This program tests XML related to WineHQ Bugzilla bug 26226,
 * i.e. SOAP-requests made by a buhl-finance.com app and BridgeCentral.
 * BridgeCentral, when using winetricks msxml3, sends this succesful login request:

     <?xml version="1.0"?>
     <SOAP-ENV:Envelope xmlns:SOAP-ENV="http://schemas.xmlsoap.org/soap/envelope/"
                        xmlns:xsd="http://www.w3.org/2001/XMLSchema"
                        xmlns:xsi="http://www.w3.org/2001/XMLSchema-instance">
         <SOAP-ENV:Body>
             <KlubLogin xmlns="http://www.wso2.org/php/xsd">
                 <klubnummer>.......</klubnummer>
                 <eksportkode>......</eksportkode>
             </KlubLogin>
         </SOAP-ENV:Body>
     </SOAP-ENV:Envelope>

 * (whitespace added for readability, and codes replaced with dots to improve security).
 * Wine msxml3 <= 1.5.4 fails after KlubLogin and never generates the rest or sends anything.
 * For the test we will attempt to generate the same output, except that we shorten "KlubLogin"
 * to "Login" and shorten the two innermost elements to a single empty "code" element <code/>
 * -- and that we expand the test to also produce a few variations of this, possibly with
 * slightly different meanings, especially to find differences between the native and the
 * Wine msxml3 behaviour.
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

#define RELEASE_ELEMENT(e) \
    do { if (e != NULL) IXMLDOMElement_Release(e); } while(0)

/* From Wine source wine/test.h (limited to about 300 chars of output, not enough for us): */
// extern const char *wine_dbgstr_wn( const WCHAR *str, int n );
// static inline const char *wine_dbgstr_w( const WCHAR *s ) { return wine_dbgstr_wn( s, -1 ); }

/* Simple hack from dlls/oleaut32/tests/vartype.c (but with increased buffer size here): */
static const char* wtoascii(LPWSTR lpszIn)
{
    static char buff[2048];
    WideCharToMultiByte(CP_ACP, 0, lpszIn, -1, buff, sizeof(buff), NULL, NULL);
    return buff;
}

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

/* Create an element in one of a multitude of possible ways, including associating a
 * namespace simultaneously with element creation and/or adding a namespace binding
 * as a reserved (xmlns[:prefix]) attribute.  Thus the namespace URI may be applied 0, 1
 * or 2 times (in 2 different ways) for the given element.
 * The xmlns_attr argument is only referenced if add_ns_as_attrib is true.
 * In that case, it should be "xmlns:PREFIX" if name has a prefix (PREFIX:localname),
 * and just "xmlns" if name has no prefix.  Other values are legally possible, but are
 * not intended for use in this test (e.g. xmlns:xsd is not created by this function).
 */
static IXMLDOMElement* create_elem_multi(IXMLDOMDocument *doc,    const char *name,
                                         const char *xmlns_attr,  const char *nsURI,
                                         BOOL use_create_element, BOOL set_nsuri_full,
                                         BOOL add_ns_as_attrib,   BOOL use_attrib_nodes)
{
    HRESULT hr;
    IXMLDOMElement* elem = NULL;

    if (use_create_element)
    {
        hr = IXMLDOMDocument_createElement(doc, _bstr_(name), &elem);
        CHK_HR("createElement (name = \"%s\")\n", name);
    }
    else
        elem = create_elem_ns(doc, name, (set_nsuri_full ? nsURI : ""));

    if (add_ns_as_attrib && elem != NULL)
        hr = set_attr(elem, xmlns_attr, nsURI, use_attrib_nodes);

CleanReturn:
    return elem;
}


#define M_USE_ATTRIB_NODES      0x0001
#define M_ADD_NS_ATTRIB_TOP     0x0002
#define M_ADD_NS_ATTRIB_INNER   0x0004

#define M_SET_ENVE_URI_FULL     0x0008
#define M_USE_ENVE_CREATE_ELEM  0x0010

#define M_SET_BODY_PREFIX       0x0020
#define M_SET_BODY_URI_FULL     0x0040
#define M_USE_BODY_CREATE_ELEM  0x0080

#define M_SET_LOGIN_URI_FULL    0x0100
#define M_USE_LOGIN_CREATE_ELEM 0x0200

#define M_SET_CODE_URI_FULL     0x0400
#define M_USE_CODE_CREATE_ELEM  0x0800

#define M_TEST_FLAGS_ALL        0x0fff

/* The `how' argument determines how elements are created and namespace bindings made
 * (howN = bit N of how).  E.g. whether namespace bindings are attempted to be made
 * via explicit attributes or not, and if so, how these (reserved xmlns) attributes are set.
 * It is absolutely relevant to test several methods, including seemingly redundant (double)
 * definition of namespace bindings, since e.g. BridgeCentral does this too.
 * "Outer" bindings means the outermost binding of a given namespace.
 * "Inner" bindings are the rest, not intended to be visible in the final XML, since
 * mentioning those would be redundant due to inheritance from parents.
 * However, initially each node is created outside the node tree, so the creating function
 * can't know whether the binding will end up being redundant or not.
 * Some combinations of flags give XML of questionable validity, but may be interesting anyway.
 *
 *   how0 = 1: Set attributes clumsily via explicit attribute nodes (like some of BridgeCentral)
 *   how0 = 0: Set attributes simply with IXMLDOMElement_setAttribute (like msdn blog example)
 *
 *   how1 = 1: Add "outer" bindings as attributes, in addition to any other ns bindings made
 *   how1 = 0: Don't add outer bindings as attributes
 *
 *   how2 = 1: Add "inner" bindings as attributes, in addition to any other ns bindings made
 *   how2 = 0: Don't add inner bindings as attributes
 *
 *   how3 = 1: Set Envelope ns URI fully at element creation time if possible (if createNode)
 *   how3 = 0: Set Envelope ns URI to empty string ("") at element creation time if possible
 *
 *   how4 = 1: Envelope made with createElement (so ns = NULL initially if current wine used)
 *   how4 = 0: Envelope made with createNode (how3 determines whether or not empty ns set)
 *
 *   how5 = 1: Body made with explicit SOAP-ENV prefix (as we really should for intended output)
 *   how5 = 0: Body made without SOAP-ENV prefix (native msxml3 may translate URI to prefix!?)
 *
 *   how6 = 1: Set Body ns URI fully at element creation time if possible (if createNode)
 *   how6 = 0: Set Body ns URI to empty string ("") at element creation time if possible
 *
 *   how7 = 1: Body made with createElement (so ns = NULL initially if current wine used)
 *   how7 = 0: Body made with createNode (how6 determines whether or not empty ns set)
 *
 *   how8 = 1: Set Login ns URI fully at element creation time if possible (if createNode)
 *   how8 = 0: Set Login ns URI to empty string ("") at element creation time if possible
 *
 *   how9 = 1: Login made with createElement (so ns = NULL initially if current wine used)
 *   how9 = 0: Login made with createNode (how8 determines whether or not empty ns set)
 *
 *   how10 = 1: Set Code ns URI fully at element creation time if possible (if createNode)
 *   how10 = 0: Set Code ns URI to empty string ("") at element creation time if possible
 *
 *   how11 = 1: Code made with createElement (so ns = NULL initially if current wine used)
 *   how11 = 0: Code made with createNode (how10 determines whether or not empty ns set)
 */

/* Try building a SOAP request step-by-step like in the Visual Basic example
 *    http://blogs.msdn.com/b/jpsanders/archive/2007/06/14/how-to-send-soap-call-using-msxml-replace-stk.aspx
 * to reproduce approximately the SOAP output of BridgeCentral w/ the native dll (winetricks).
 * Wine's msxml3 currently chokes on the calls made by BridgeCentral, spoiling its SOAP login.
 * The how argument is interpreted as explained above.
 */
static void test_build_soap(IXMLDOMDocument *doc, int how)
{
    HRESULT hr;
    IXMLDOMProcessingInstruction *nodePI = NULL;
    IXMLDOMElement *soapEnvelope = NULL, *soapBody = NULL, *soapCall = NULL, *soapArg1 = NULL;

    BSTR xml;
    BOOL use_an   = ((how & M_USE_ATTRIB_NODES) != 0);
    BOOL add_nsa1 = ((how & M_ADD_NS_ATTRIB_TOP) != 0);
    BOOL add_nsa2 = ((how & M_ADD_NS_ATTRIB_INNER) != 0);

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
        goto CleanReturn;
    }
    hr = IXMLDOMDocument_appendChild(doc, (IXMLDOMNode*)nodePI, NULL);
    if(hr != S_OK) printf("appending processing instruction as child to doc failed\n");

    IXMLDOMProcessingInstruction_Release(nodePI);


    soapEnvelope = create_elem_multi(doc, "SOAP-ENV:Envelope", "xmlns:SOAP-ENV",
                                     "http://schemas.xmlsoap.org/soap/envelope/",
                                     ((how & M_USE_ENVE_CREATE_ELEM) != 0),
                                     ((how & M_SET_ENVE_URI_FULL) != 0), add_nsa1, use_an);

    if (soapEnvelope == NULL) goto CleanReturn;

    hr = set_attr(soapEnvelope, "xmlns:xsd", "http://www.w3.org/2001/XMLSchema", use_an);
    hr = set_attr(soapEnvelope, "xmlns:xsi", "http://www.w3.org/2001/XMLSchema-instance",
                  use_an);

    hr = IXMLDOMDocument_appendChild(doc, (IXMLDOMNode*)soapEnvelope, NULL);
    if(hr != S_OK) printf("appending SOAP envelope as child to doc failed\n");

    soapBody = create_elem_multi(doc,
                                 ((how & M_SET_BODY_PREFIX) ? "SOAP-ENV:Body"  : "Body"),
                                 ((how & M_SET_BODY_PREFIX) ? "xmlns:SOAP-ENV" : "xmlns"),
                                 "http://schemas.xmlsoap.org/soap/envelope/",
                                 ((how & M_USE_BODY_CREATE_ELEM) != 0),
                                 ((how & M_SET_BODY_URI_FULL) != 0), add_nsa2, use_an);

    if (soapBody == NULL) goto CleanReturn;

    hr = IXMLDOMElement_appendChild(soapEnvelope, (IXMLDOMNode*)soapBody, NULL);
    if(hr != S_OK) printf("appending SOAP body as child to envelope failed\n");

    soapCall = create_elem_multi(doc, "Login", "xmlns", "http://www.wso2.org/php/xsd",
                                     ((how & M_USE_LOGIN_CREATE_ELEM) != 0),
                                     ((how & M_SET_LOGIN_URI_FULL) != 0), add_nsa1, use_an);

    if (soapCall == NULL) goto CleanReturn;

    hr = IXMLDOMElement_appendChild(soapBody, (IXMLDOMNode*)soapCall, NULL);
    if(hr != S_OK) printf("appending SOAP call as child to body failed\n");



    soapArg1 = create_elem_multi(doc, "code", "xmlns", "http://www.wso2.org/php/xsd",
                                     ((how & M_USE_CODE_CREATE_ELEM) != 0),
                                     ((how & M_SET_CODE_URI_FULL) != 0), add_nsa2, use_an);

    if (soapArg1 == NULL) goto CleanReturn;

    hr = IXMLDOMElement_appendChild(soapCall, (IXMLDOMNode*)soapArg1, NULL);
    if(hr != S_OK) printf("appending SOAP arg1 as child to call failed\n");

    hr = IXMLDOMDocument_get_xml(doc, &xml);
    if(hr == S_OK)
    {
        // printf("dbgstr(XML(%0x)) = %s\n", how, wine_dbgstr_w(xml));
        printf("========== Generated XML in free form: ============\n%s%s",
               wtoascii(xml),
               "===================================================\n");
    }
    else
        printf("Getting back the XML failed\n");
    SysFreeString(xml);

CleanReturn:
    RELEASE_ELEMENT(soapEnvelope);
    RELEASE_ELEMENT(soapBody);
    RELEASE_ELEMENT(soapCall);
    RELEASE_ELEMENT(soapArg1);
    free_bstrs();
}

int main(int argc, char **argv)
{
    int how;
    IXMLDOMDocument *doc;
    HRESULT hr;

    if (argc != 2 || ((how = atoi(argv[1])) < 0 || how > M_TEST_FLAGS_ALL))
    {
        printf("Usage: %s HOW\n  where HOW is an integer 0..%d\n%s\n",
               argv[0], M_TEST_FLAGS_ALL,
               "  Some interesting values to test: 2738, 2739, 1384, 1398, 1399");
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
    printf("DOMDocument successfully created\n");

    test_build_soap(doc, how);

    IXMLDOMDocument_Release(doc);
    CoUninitialize();
    return 0;
}
