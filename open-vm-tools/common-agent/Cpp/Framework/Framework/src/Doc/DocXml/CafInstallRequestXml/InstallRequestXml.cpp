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

#include "stdafx.h"

#include "Doc/DocXml/CafCoreTypesXml/AttachmentCollectionXml.h"
#include "Doc/DocXml/CafCoreTypesXml/RequestHeaderXml.h"
#include "Doc/DocXml/CafInstallRequestXml/InstallBatchXml.h"

#include "Doc/CafCoreTypesDoc/CAttachmentCollectionDoc.h"
#include "Doc/CafCoreTypesDoc/CRequestHeaderDoc.h"
#include "Doc/CafInstallRequestDoc/CInstallBatchDoc.h"
#include "Doc/CafInstallRequestDoc/CInstallRequestDoc.h"
#include "Xml/XmlUtils/CXmlElement.h"
#include "Doc/DocXml/CafInstallRequestXml/InstallRequestXml.h"

using namespace Caf;

void InstallRequestXml::add(
	const SmartPtrCInstallRequestDoc installRequestDoc,
	const SmartPtrCXmlElement thisXml) {
	CAF_CM_STATIC_FUNC_VALIDATE("InstallRequestXml", "add");

	CAF_CM_ENTER {
		CAF_CM_VALIDATE_SMARTPTR(installRequestDoc);
		CAF_CM_VALIDATE_SMARTPTR(thisXml);

		const std::string clientIdVal =
			BasePlatform::UuidToString(installRequestDoc->getClientId());
		CAF_CM_VALIDATE_STRING(clientIdVal);
		thisXml->addAttribute("clientId", clientIdVal);

		const std::string requestIdVal =
			BasePlatform::UuidToString(installRequestDoc->getRequestId());
		CAF_CM_VALIDATE_STRING(requestIdVal);
		thisXml->addAttribute("requestId", requestIdVal);

		const std::string pmeIdVal = installRequestDoc->getPmeId();
		CAF_CM_VALIDATE_STRING(pmeIdVal);
		thisXml->addAttribute("pmeId", pmeIdVal);

		const SmartPtrCRequestHeaderDoc requestHeaderVal =
			installRequestDoc->getRequestHeader();
		CAF_CM_VALIDATE_SMARTPTR(requestHeaderVal);

		const SmartPtrCXmlElement requestHeaderXml =
			thisXml->createAndAddElement("requestHeader");
		RequestHeaderXml::add(requestHeaderVal, requestHeaderXml);

		const SmartPtrCInstallBatchDoc batchVal =
			installRequestDoc->getBatch();
		CAF_CM_VALIDATE_SMARTPTR(batchVal);

		const SmartPtrCXmlElement batchXml =
			thisXml->createAndAddElement("batch");
		InstallBatchXml::add(batchVal, batchXml);

		const SmartPtrCAttachmentCollectionDoc attachmentCollectionVal =
			installRequestDoc->getAttachmentCollection();
		if (! attachmentCollectionVal.IsNull()) {
			const SmartPtrCXmlElement attachmentCollectionXml =
				thisXml->createAndAddElement("attachmentCollection");
			AttachmentCollectionXml::add(attachmentCollectionVal, attachmentCollectionXml);
		}
	}
	CAF_CM_EXIT;
}

SmartPtrCInstallRequestDoc InstallRequestXml::parse(
	const SmartPtrCXmlElement thisXml) {
	CAF_CM_STATIC_FUNC_VALIDATE("InstallRequestXml", "parse");

	SmartPtrCInstallRequestDoc installRequestDoc;

	CAF_CM_ENTER {
		CAF_CM_VALIDATE_SMARTPTR(thisXml);

		const std::string clientIdStrVal =
			thisXml->findRequiredAttribute("clientId");
		UUID clientIdVal = CAFCOMMON_GUID_NULL;
		if (! clientIdStrVal.empty()) {
			BasePlatform::UuidFromString(clientIdStrVal.c_str(), clientIdVal);
		}

		const std::string requestIdStrVal =
			thisXml->findRequiredAttribute("requestId");
		UUID requestIdVal = CAFCOMMON_GUID_NULL;
		if (! requestIdStrVal.empty()) {
			BasePlatform::UuidFromString(requestIdStrVal.c_str(), requestIdVal);
		}

		const std::string pmeIdVal =
			thisXml->findRequiredAttribute("pmeId");

		const SmartPtrCXmlElement requestHeaderXml =
			thisXml->findRequiredChild("requestHeader");

		SmartPtrCRequestHeaderDoc requestHeaderVal;
		if (! requestHeaderXml.IsNull()) {
			requestHeaderVal = RequestHeaderXml::parse(requestHeaderXml);
		}

		const SmartPtrCXmlElement batchXml =
			thisXml->findRequiredChild("batch");

		SmartPtrCInstallBatchDoc batchVal;
		if (! batchXml.IsNull()) {
			batchVal = InstallBatchXml::parse(batchXml);
		}

		const SmartPtrCXmlElement attachmentCollectionXml =
			thisXml->findOptionalChild("attachmentCollection");

		SmartPtrCAttachmentCollectionDoc attachmentCollectionVal;
		if (! attachmentCollectionXml.IsNull()) {
			attachmentCollectionVal = AttachmentCollectionXml::parse(attachmentCollectionXml);
		}

		installRequestDoc.CreateInstance();
		installRequestDoc->initialize(
			clientIdVal,
			requestIdVal,
			pmeIdVal,
			requestHeaderVal,
			batchVal,
			attachmentCollectionVal);
	}
	CAF_CM_EXIT;

	return installRequestDoc;
}

