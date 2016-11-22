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

#ifndef InstanceOperationXml_h_
#define InstanceOperationXml_h_


#include "Doc/SchemaTypesDoc/CInstanceOperationDoc.h"

#include "Doc/DocXml/SchemaTypesXml/SchemaTypesXmlLink.h"
#include "Xml/XmlUtils/CXmlElement.h"

namespace Caf {

	/// Streams the InstanceOperation class to/from XML
	namespace InstanceOperationXml {

		/// Adds the InstanceOperationDoc into the XML.
		void SCHEMATYPESXML_LINKAGE add(
			const SmartPtrCInstanceOperationDoc instanceOperationDoc,
			const SmartPtrCXmlElement thisXml);

		/// Parses the InstanceOperationDoc from the XML.
		SmartPtrCInstanceOperationDoc SCHEMATYPESXML_LINKAGE parse(
			const SmartPtrCXmlElement thisXml);
	}
}

#endif
