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

#include "windows.h"

#include "msxml2.h"
#include "msxml2did.h"
#include "ole2.h"
#include "dispex.h"

#include "initguid.h"
#include "objsafe.h"
#include "mshtml.h"

/* undef the #define in msxml2 so that we can access all versions */
#undef CLSID_DOMDocument

/* Parts of this file come from Wine's dlls/msxml3/tests/domdoc.c */


int main(void)
{
    IXMLDOMDocument *doc;
    IUnknown *unk;
    HRESULT hr;

    hr = CoInitialize( NULL );

    if (hr == S_OK)
      printf("CoInitialize successful!\n");
    else
      printf("Failed to init com\n");

    if (hr != S_OK) return 1;
#if 0
    hr = CoCreateInstance( &CLSID_DOMDocument, NULL, CLSCTX_INPROC_SERVER, &IID_IXMLDOMDocument, (void**)&doc );
    if (hr != S_OK)
    {
        printf("IXMLDOMDocument is not available (0x%08x)\n", hr);
        return 1;
    }
    printf("DOMDocument succesfully created\n");
    // IXMLDOMDocument_Release(doc);
    //doc->Release();
#endif
    return 0;
}
