/*
 *  Author: bwilliams
 *  Created: April 6, 2012
 *
 *  Copyright (C) 2012-2016 VMware, Inc.  All rights reserved. -- VMware Confidential
 *
 *  This code was generated by the script "build/dev/codeGen/genCppXml". Please
 *  speak to Brian W. before modifying it by hand.
 *
 */

#ifndef MethodParameterXml_h_
#define MethodParameterXml_h_


#include "Doc/SchemaTypesDoc/CMethodParameterDoc.h"

#include "Doc/DocXml/SchemaTypesXml/SchemaTypesXmlLink.h"
#include "Xml/XmlUtils/CXmlElement.h"

namespace Caf {

	/// Streams the MethodParameter class to/from XML
	namespace MethodParameterXml {

		/// Adds the MethodParameterDoc into the XML.
		void SCHEMATYPESXML_LINKAGE add(
			const SmartPtrCMethodParameterDoc methodParameterDoc,
			const SmartPtrCXmlElement thisXml);

		/// Parses the MethodParameterDoc from the XML.
		SmartPtrCMethodParameterDoc SCHEMATYPESXML_LINKAGE parse(
			const SmartPtrCXmlElement thisXml);
	}
}

#endif
