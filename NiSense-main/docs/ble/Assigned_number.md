# **Assigned Numbers** 

##### **_Bluetooth_**<sup>**_®_**</sup> **Document** 

▪ **Version Date:** 2026-06-23 

###### **Abstract:** 

This is a regularly updated document listing assigned numbers, codes, and identifiers in the Bluetooth specifications. 

Bluetooth SIG Proprietary 

**Assigned Numbers** / Document 

**2026-06-23** 

**This document, regardless of its title or content, is not a Bluetooth Specification as defined in the Bluetooth Patent/Copyright License Agreement (“PCLA”) and Bluetooth Trademark License Agreement. Use of this document by members of Bluetooth SIG is governed by the membership and other related agreements between Bluetooth SIG Inc. (“Bluetooth SIG”) and its members, including the PCLA and other agreements posted on Bluetooth SIG’s website located at www.bluetooth.com.** 

**THIS DOCUMENT IS PROVIDED “AS IS” AND BLUETOOTH SIG, ITS MEMBERS, AND THEIR AFFILIATES MAKE NO REPRESENTATIONS OR WARRANTIES AND DISCLAIM ALL WARRANTIES, EXPRESS OR IMPLIED, INCLUDING ANY WARRANTY OF MERCHANTABILITY, TITLE, NON-INFRINGEMENT, FITNESS FOR ANY PARTICULAR PURPOSE, THAT THE CONTENT OF THIS DOCUMENT IS FREE OF ERRORS.** 

**TO THE EXTENT NOT PROHIBITED BY LAW, BLUETOOTH SIG, ITS MEMBERS, AND THEIR AFFILIATES DISCLAIM ALL LIABILITY ARISING OUT OF OR RELATING TO USE OF THIS DOCUMENT AND ANY INFORMATION CONTAINED IN THIS DOCUMENT, INCLUDING LOST REVENUE, PROFITS, DATA OR PROGRAMS, OR BUSINESS INTERRUPTION, OR FOR SPECIAL, INDIRECT, CONSEQUENTIAL, INCIDENTAL OR PUNITIVE DAMAGES, HOWEVER CAUSED AND REGARDLESS OF THE THEORY OF LIABILITY, AND EVEN IF BLUETOOTH SIG, ITS MEMBERS, OR THEIR AFFILIATES HAVE BEEN ADVISED OF THE POSSIBILITY OF SUCH DAMAGES.** 

**This document is proprietary to Bluetooth SIG. This document may contain or cover subject matter that is intellectual property of Bluetooth SIG and its members. The furnishing of this document does not grant any license to any intellectual property of Bluetooth SIG or its members.** 

**This document is subject to change without notice.** 

**Copyright © 2020–2026 by Bluetooth SIG, Inc. The Bluetooth word mark and logos are owned by Bluetooth SIG, Inc. Other third-party brands and names are the property of their respective owners.** 

Bluetooth SIG Proprietary 

Page 2 of 442 

**Assigned Numbers** / Document 

**2026-06-23** 

### **Contents** 

|**1**<br>**Introduction . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . 8**|
|---|
|**2**<br>**Core Specification . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . 9**|
|2.1<br>Core specification versions . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . 10|
|2.2<br>Special LAPs. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .11|
|2.3<br>Common Data Types. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .12|
|2.4<br>Characteristic Presentation Format . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . 15|
|2.4.1<br>GATT Format Types. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .15|
|2.4.2<br>GATT Characteristic Presentation Format Name Space. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .17|
|2.4.2.1<br>Bluetooth SIG GATT Characteristic Presentation Format Description. . . . . . . . . . . . . . . . . . . . . . . . . . . .18|
|2.5<br>PSMs and SPSMs . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . 26|
|2.6<br>Appearance Values . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . 27|
|2.6.1<br>Definition. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .27|
|2.6.2<br>Appearance Category ranges . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .28|
|2.6.3<br>Appearance Sub-category values. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .30|
|2.7<br>URI Scheme Name String Mapping. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .40|
|2.8<br>Class of Device . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . 46|
|2.8.1<br>Major Service Classes. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .46|
|2.8.2<br>Major Device Class. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .47|
|2.8.2.1<br>Minor Device Class field – Computer Major Class. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .48|
|2.8.2.2<br>Minor Device Class field – Phone Major Class . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .49|
|2.8.2.3<br>Minor Device Class field – LAN/Network Access Point Major Class . . . . . . . . . . . . . . . . . . . . . . . . . . . . .50|
|2.8.2.4<br>Minor Device Class field – Audio/Video Major Class . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .51|
|2.8.2.5<br>Minor Device Class field – Peripheral Major Class . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .52|
|2.8.2.6<br>Minor Device Class field – Imaging Major Class. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .53|
|2.8.2.7<br>Minor Device Class field – Wearable Major Class . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .54|
|2.8.2.8<br>Minor Device Class field – Toy Major Class . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .55|
|2.8.2.9<br>Minor Device Class field – Health Major Class . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .56|
|2.9<br>LE-U CID . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . 57|
|2.10<br>PCM_Data_Format . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . 58|
|2.11<br>Coding_Format and Codec_ID (HCI) . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . 59|
|2.12<br>MWS . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . 60|
|2.12.1<br>MWS Coexistence Transport Layer. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .60|
|2.12.2<br>MWS Channel Type. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .60|
|2.13<br>AMP . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . 61|
|2.13.1<br>AMP Controller Type . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .61|
|2.13.2<br>AMP Security keyIDs. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .61|
|2.13.3<br>AMP Security keyLength . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .61|
|2.13.4<br>PAL Versions . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .61|
|**3**<br>**16-bit UUIDs . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . 62**|
|3.1<br>Protocol Identifiers . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . 62|
|3.2<br>Browse Group Identifiers . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . 63|



Bluetooth SIG Proprietary 

Page 3 of 442 

**Assigned Numbers** / Document 

**2026-06-23** 

|3.3<br>SDP Service Class and Profile Identifiers . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . 64|
|---|
|3.4<br>GATT Services . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . 67|
|3.4.1<br>Services by Name. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .67|
|3.4.2<br>Services by UUID . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .70|
|3.5<br>Units . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . 73|
|3.5.1<br>Units by Name . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .73|
|3.5.2<br>Units by UUID. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .77|
|3.6<br>Declarations . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . 81|
|3.7<br>Descriptors. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .82|
|3.8<br>Characteristics . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . 83|
|3.8.1<br>Characteristics by Name . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .83|
|3.8.2<br>Characteristics by UUID . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .98|
|3.9<br>Object Types. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .113|
|3.9.1<br>Object Types by Name . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .113|
|3.9.2<br>Object Types by UUID . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .114|
|3.10<br>SDO Services . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . 115|
|3.11<br>Member Services . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . 116|
|3.12<br>Mesh Profiles. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .136|
|**4**<br>**Mesh . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . 137**|
|4.1<br>Mesh Model Identifiers . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . 138|
|4.1.1<br>Mesh Model Identifiers by Value. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .138|
|4.1.2<br>Mesh Model Identifiers by Name . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .140|
|4.2<br>Mesh Model Message Opcodes . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . 143|
|4.2.1<br>Mesh Model Message Opcodes by Value. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .143|
|4.2.2<br>Mesh Model Message Opcodes by Name . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .155|
|4.3<br>Mesh Protocol . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . 168|
|4.3.1<br>Mesh Beacon Types . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .168|
|4.3.2<br>Mesh Transport Control Message Opcodes . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .168|
|4.3.3<br>Mesh Provisioning PDU Types . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .169|
|4.3.4<br>Mesh Proxy PDU Types. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .170|
|4.3.5<br>Mesh Health Fault IDs . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .171|
|4.3.5.1<br>Mesh Health Fault IDs by Value . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .171|
|4.3.5.2<br>Mesh Health Fault IDs by Name. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .172|
|4.3.6<br>Mesh Metadata Identifiers. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .174|
|4.4<br>Mesh Model . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . 175|
|4.4.1<br>Light Purpose. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .175|
|**5**<br>**Service Discovery . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . 176**|
|5.1<br>Attribute Identifiers . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . 176|
|5.1.1<br>Advanced Audio Distribution Profile (A2DP) . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .176|
|5.1.2<br>Audio/Video Remote Control Profile (AVRCP) . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .176|
|5.1.3<br>Basic Imaging Profile (BIP). . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .176|
|5.1.4<br>Basic Printing Profile (BPP) . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .177|
|5.1.5<br>Bluetooth Core Specification: Universal Attributes . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .178<br>Bluetooth SIG Proprietary<br>Page 4 of 442|



**Assigned Numbers** / Document 

**2026-06-23** 

|5.1.6|Bluetooth Core Specification: Service Discovery Service . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .178|
|---|---|
|5.1.7|Bluetooth Core Specification: Browse Group Descriptor Service . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .179|
|5.1.8|Calendar Tasks and Notes (CTN) . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .179|
|5.1.9|Cordless Telephony Profile (CTP) . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .179|
|5.1.10|Device Identification Profile (DID) . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .179|
|5.1.11|Dial-Up Networking Profile (DUN) . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .180<br>|
|5.1.12|FAX Profile (FAX). . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .180|
|5.1.13|File Transfer Profile (FTP) . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .180|
|5.1.14|Global Navigation Satellite System Profile (GNSS) . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .181|
|5.1.15|Hands-Free Profile (HFP). . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .181|
|5.1.16|Hardcopy Replacement Profile (HCRP) . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .181|
|5.1.17|Headset Profile (HSP) . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .182|
|5.1.18|Health Device Profile (HDP) . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .182|
|5.1.19|Human Interface Device Profile (HID) . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .182|
|5.1.20|Interoperability Requirements for Bluetooth technology as a WAP Bearer (WAP) . . . . . . . . . . . . . . . . .183|
|5.1.21|Message Access Profile (MAP) . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .183|
|5.1.22|Multi-Profile Specification (MPS) . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .184|
|5.1.23|Object Push Profile (OPP) . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .184|
|5.1.24|Personal Area Network Profile (PAN) . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .184|
|5.1.25|Phone Book Access Profile (PBAP) . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .185|
|5.1.26|Synchronization Profile (SYNC) . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .185|
|5.2<br>Attr|ibute ID Offsets for Strings. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .185|
|5.3<br>Pro|tocol Parameters . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . 186|
|**6**<br>**Profiles**|**and Services. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .187**|
|6.1<br>Env|ironmental Sensing Service . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . 187|
|6.1.1|Permitted Characteristics . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .187|
|6.2<br>Use|r Data Service . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . 187|
|6.2.1|Permitted Characteristics . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .187|
|6.3<br>A/V|Distribution Protocol (AVDTP). . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .189|
|6.3.1|Media Type . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .189|
|6.3.2|A/V Content Protection Method . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .189|
|6.4<br>A/V|Remote Control Profile (AVRCP). . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .190|
|6.4.1|Major Player Type . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .190|
|6.4.2|Player Sub Type . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .190|
|6.4.3|Folder Type. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .190|
|6.4.4|Media Type . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .191|
|6.4.5|List of Media Attributes. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .191|
|6.4.6|Player Application Settings. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .191|
|6.5<br>Adv|anced Audio Distribution Profile (A2DP) . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . 192|
|6.5.1|Audio Codec ID . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .192|
|6.6<br>Vid|eo Distribution Profile (VDP) . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . 192|
|6.6.1|Video Codec ID . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .192|
|6.7<br>Tra|nsport Discovery Service . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . 194|
|6.7.1<br>Blueto|Organization IDs . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .194<br>oth SIG Proprietary<br>Page 5 of 442|



**Assigned Numbers** / Document 

**2026-06-23** 

|6.8<br>Health Device Profile. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .195|
|---|
|6.8.1<br>Data Exchange Specifications. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .195|
|6.8.2<br>Device Data Specializations. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .195|
|6.9<br>Message Access Profile. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .199|
|6.9.1<br>Presence . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .199|
|6.9.2<br>Chat State . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .199|
|6.9.3<br>Message Extended Data . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .199|
|6.10<br>Hands-Free Profile . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . 201|
|6.10.1<br>HF Indicators . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .201|
|6.10.2<br>Bearer Technology. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .201|
|6.10.3<br>Uniform Caller Identifiers. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .201|
|6.11<br>Telephone Bearer Service. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .202|
|6.11.1<br>Uniform Caller Identifiers. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .202|
|6.12<br>Generic Audio . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . 204|
|6.12.1<br>Audio Location Definitions . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .204|
|6.12.2<br>Audio Input Type Definitions . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .205|
|6.12.3<br>Context Type . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .205|
|6.12.4<br>Codec_Specific_Capabilities LTV Structures . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .206|
|6.12.4.1<br>Supported_Sampling_Frequencies . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .206|
|6.12.4.2<br>Supported_Frame_Durations . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .206|
|6.12.4.3<br>Supported_Audio_Channel_Counts. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .207|
|6.12.4.4<br>Supported_Octets_Per_Codec_Frame. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .207|
|6.12.4.5<br>Supported_Max_Codec_Frames_Per_SDU. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .208|
|6.12.5<br>Codec_Specific_Configuration LTV structures. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .208|
|6.12.5.1<br>Sampling_Frequency . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .208|
|6.12.5.2<br>Frame_Duration . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .209|
|6.12.5.3<br>Audio_Channel_Allocation . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .209|
|6.12.5.4<br>Octets_Per_Codec_Frame . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .209|
|6.12.5.5<br>Codec_Frame_Blocks_Per_SDU . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .209|
|6.12.6<br>Metadata LTV structures. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .210|
|6.12.6.1<br>Preferred_Audio_Contexts. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .210|
|6.12.6.2<br>Streaming_Audio_Contexts. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .210|
|6.12.6.3<br>Program_Info . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .211|
|6.12.6.4<br>Language. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .211|
|6.12.6.5<br>CCID_List . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .211|
|6.12.6.6<br>Parental_Rating . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .212|
|6.12.6.7<br>Program_Info_URI. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .212|
|6.12.6.8<br>Extended_Metadata . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .212|
|6.12.6.9<br>Vendor_Specific . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .213|
|6.12.6.10<br>Audio_Active_State . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .213|
|6.12.6.11<br>Broadcast_Audio_Immediate_Rendering_Flag. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .213|
|6.12.6.12<br>Assisted_Listening_Stream. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .213|
|6.12.6.13<br>Broadcast_Name. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .214|
|6.13<br>Electronic Shelf Label Service . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . 215|



Bluetooth SIG Proprietary 

Page 6 of 442 

**Assigned Numbers** / Document 

**2026-06-23** 

|6.13.1<br>Display Types . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .215|
|---|
|6.14<br>Industrial Measurement Device Service . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . 216|
|6.14.1<br>Permitted Characteristics . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .216|
|6.15<br>Cookware Service . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . 217|
|6.15.1<br>Permitted Characteristics . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .217|
|**7**<br>**Company Identifiers . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . 218**|
|7.1<br>Company Identifiers by Value . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . 218|
|7.2<br>Company Identifiers by Name . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . 330|
|**8**<br>**References . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . 442**|



Bluetooth SIG Proprietary 

Page 7 of 442 

**Assigned Numbers** / Document 

**2026-06-23** 

## **<u>1 Introduction</u>** 

This is a regularly updated document listing assigned numbers, codes, and identifiers in the Bluetooth specifications. This document is referenced by the Bluetooth Core Specification, Volume 1, Part E, Section 2.6 [4], and other profile and service specifications. 

Any value defined in this document is reserved for that use by the Bluetooth SIG within the scope of a Bluetooth specification. 

Any values not defined in this document for a given assigned numbers type are Reserved for Future Use (RFU) and shall be handled as specified in the Bluetooth Core Specification, Volume 1, Part E, Section 2.4 [4]. 

Bluetooth SIG Proprietary 

Page 8 of 442 

**Assigned Numbers** / Document 

**2026-06-23** 

**2 Core S ecification** **<u>p</u>** 

The following section includes the assigned numbers defined by the Core Specification [4]. The subsections are organized by volume and part within the Core Specification. 

Bluetooth SIG Proprietary 

Page 9 of 442 

**Assigned Numbers** / Document 

**2026-06-23** 

#### **2.1 Core specification versions** 

Referenced from the following: 

- Bluetooth Core Specification [Vol 2] Part C, Section 5.2 [4]. 

- Bluetooth Core Specification [Vol 4] Part E, Section 7.4.1 [4]. 

- Bluetooth Core Specification [Vol 4] Part E, Section 7.7.12 [4]. 

- Bluetooth Core Specification [Vol 6] Part B, Section 2.4.2.13 [4]. 

###### Last Modified: 2026-05-08 

Filename: assigned_numbers/core/core_version.yaml 

|**Core Specification Name**|**Version**|
|---|---|
|Bluetooth® Core Specification 1.0b (Withdrawn)|0x00|
|Bluetooth® Core Specification 1.1 (Withdrawn)|0x01|
|Bluetooth® Core Specification 1.2 (Withdrawn)|0x02|
|Bluetooth® Core Specification 2.0+EDR (Withdrawn)|0x03|
|Bluetooth® Core Specification 2.1+EDR (Withdrawn)|0x04|
|Bluetooth® Core Specification 3.0+HS (Withdrawn)|0x05|
|Bluetooth® Core Specification 4.0 (Withdrawn)|0x06|
|Bluetooth® Core Specification 4.1 (Deprecated)|0x07|
|Bluetooth® Core Specification 4.2|0x08|
|Bluetooth® Core Specification 5.0|0x09|
|Bluetooth® Core Specification 5.1|0x0A|
|Bluetooth® Core Specification 5.2|0x0B|
|Bluetooth® Core Specification 5.3|0x0C|
|Bluetooth® Core Specification 5.4|0x0D|
|Bluetooth® Core Specification 6.0|0x0E|
|Bluetooth® Core Specification 6.1|0x0F|
|Bluetooth® Core Specification 6.2|0x10|
|Bluetooth® Core Specification 6.3|0x11|



Bluetooth SIG Proprietary 

Page 10 of 442 

**Assigned Numbers** / Document 

**2026-06-23** 

#### **2.2 Special LAPs** 

Referenced from the following: 

- Bluetooth Core Specification [Vol 1] Part B, Section 1 [4]. 

- Bluetooth Core Specification [Vol 2] Part B, Section 1.2.1 [4]. 

- Bluetooth Core Specification [Vol 4] Part E, Section 7.1.1 [4]. 

- Bluetooth Core Specification [Vol 4] Part E, Section 7.1.3 [4]. 

- Bluetooth Core Specification [Vol 4] Part E, Section 7.7.74 [4]. 

###### Last Modified: 2024-02-05 

Filename: assigned_numbers/core/diacs.yaml 

|**Name**|**Value**|
|---|---|
|Limited Inquiry Access Code (LIAC)|0x9E8B00|
|General Inquiry Access Code (GIAC)|0x9E8B33|



Bluetooth SIG Proprietary 

Page 11 of 442 

**Assigned Numbers** / Document 

**2026-06-23** 

#### **2.3 Common Data Types** 

Referenced from the following: 

- Supplement to the Bluetooth Core Specification Part A, Section 1 [22]. 

- Bluetooth Core Specification [Vol 3] Part C, Section 8 [4]. 

- Bluetooth Core Specification [Vol 3] Part C, Section 11 [4]. 

- Bluetooth Core Specification [Vol 6] Part B, Section 2.3.4.8 [4]. 

###### Last Modified: 2024-06-13 

Filename: assigned_numbers/core/ad_types.yaml 

|**Common Data**<br>**Type**|**Name**|**Reference**|
|---|---|---|
|0x01|Flags|Core Specification Supplement, Part<br>A, Section 1.3|
|0x02|Incomplete List of 16-bit Service or<br>Service Class UUIDs|Core Specification Supplement, Part<br>A, Section 1.1|
|0x03|Complete List of 16-bit Service or<br>Service Class UUIDs|Core Specification Supplement, Part<br>A, Section 1.1|
|0x04|Incomplete List of 32-bit Service or<br>Service Class UUIDs|Core Specification Supplement, Part<br>A, Section 1.1|
|0x05|Complete List of 32-bit Service or<br>Service Class UUIDs|Core Specification Supplement, Part<br>A, Section 1.1|
|0x06|Incomplete List of 128-bit Service or<br>Service Class UUIDs|Core Specification Supplement, Part<br>A, Section 1.1|
|0x07|Complete List of 128-bit Service or<br>Service Class UUIDs|Core Specification Supplement, Part<br>A, Section 1.1|
|0x08|Shortened Local Name|Core Specification Supplement, Part<br>A, Section 1.2|
|0x09|Complete Local Name|Core Specification Supplement, Part<br>A, Section 1.2|
|0x0A|Tx Power Level|Core Specification Supplement, Part<br>A, Section 1.5|
|0x0D|Class of Device|Core Specification Supplement, Part<br>A, Section 1.6|
|0x0E|Simple Pairing Hash C-192|Core Specification Supplement, Part<br>A, Section 1.6|
|0x0F|Simple Pairing Randomizer R-192|Core Specification Supplement, Part<br>A, Section 1.6|
|0x10|Device ID|Device ID Profile (when used in EIR<br>data)|
|0x10|Security Manager TK Value|Core Specification Supplement, Part<br>A, Section 1.8 (when used in OOB<br>data blocks)|
|0x11|Security Manager Out of Band Flags|Core Specification Supplement, Part<br>A, Section 1.7|



Bluetooth SIG Proprietary 

Page 12 of 442 

**Assigned Numbers** / Document 

**2026-06-23** 

|0x12|Peripheral Connection Interval Range|Core Specification Supplement, Part<br>A, Section 1.9|
|---|---|---|
|0x14|List of 16-bit Service Solicitation<br>UUIDs|Core Specification Supplement, Part<br>A, Section 1.10|
|0x15|List of 128-bit Service Solicitation<br>UUIDs|Core Specification Supplement, Part<br>A, Section 1.10|
|0x16|Service Data - 16-bit UUID|Core Specification Supplement, Part<br>A, Section 1.11|
|0x17|Public Target Address|Core Specification Supplement, Part<br>A, Section 1.13|
|0x18|Random Target Address|Core Specification Supplement, Part<br>A, Section 1.14|
|0x19|Appearance|Core Specification Supplement, Part<br>A, Section 1.12|
|0x1A|Advertising Interval|Core Specification Supplement, Part<br>A, Section 1.15|
|0x1B|LE Bluetooth Device Address|Core Specification Supplement, Part<br>A, Section 1.16|
|0x1C|LE Role|Core Specification Supplement, Part<br>A, Section 1.17|
|0x1D|Simple Pairing Hash C-256|Core Specification Supplement, Part<br>A, Section 1.6|
|0x1E|Simple Pairing Randomizer R-256|Core Specification Supplement, Part<br>A, Section 1.6|
|0x1F|List of 32-bit Service Solicitation<br>UUIDs|Core Specification Supplement, Part<br>A, Section 1.10|
|0x20|Service Data - 32-bit UUID|Core Specification Supplement, Part<br>A, Section 1.11|
|0x21|Service Data - 128-bit UUID|Core Specification Supplement, Part<br>A, Section 1.11|
|0x22|LE Secure Connections Confirmation<br>Value|Core Specification Supplement, Part<br>A, Section 1.6|
|0x23|LE Secure Connections Random<br>Value|Core Specification Supplement, Part<br>A, Section 1.6|
|0x24|URI|Core Specification Supplement, Part<br>A, Section 1.18|
|0x25|Indoor Positioning|Indoor Positioning Service|
|0x26|Transport Discovery Data|Transport Discovery Service|
|0x27|LE Supported Features|Core Specification Supplement, Part<br>A, Section 1.19|
|0x28|Channel Map Update Indication|Core Specification Supplement, Part<br>A, Section 1.20|



Bluetooth SIG Proprietary 

Page 13 of 442 

**Assigned Numbers** / Document 

**2026-06-23** 

|0x29|PB-ADV|Mesh Profile Specification, Section<br>5.2.1|
|---|---|---|
|0x2A|Mesh Message|Mesh Profile Specification, Section<br>3.3.1|
|0x2B|Mesh Beacon|Mesh Profile Specification, Section 3.9|
|0x2C|BIGInfo|Core Specification Supplement, Part<br>A, Section 1.21|
|0x2D|Broadcast_Code|Core Specification Supplement, Part<br>A, Section 1.22|
|0x2E|Resolvable Set Identifier|Coordinated Set Identification Profile<br>v1.0 or later|
|0x2F|Advertising Interval - long|Core Specification Supplement, Part<br>A, Section 1.15|
|0x30|Broadcast_Name|Public Broadcast Profile v1.0 or later|
|0x31|Encrypted Advertising Data|Core Specification Supplement, Part<br>A, Section 1.23|
|0x32|Periodic Advertising Response Timing<br>Information|Core Specification Supplement, Part<br>A, Section 1.24|
|0x34|Electronic Shelf Label|ESL Profile|
|0x3D|3D Information Data|3D Synchronization Profile|
|0xFF|Manufacturer Specific Data|Core Specification Supplement, Part<br>A, Section 1.4|



Bluetooth SIG Proprietary 

Page 14 of 442 

**Assigned Numbers** / Document 

**2026-06-23** 

#### **2.4 Characteristic Presentation Format** 

See Bluetooth Core Specification [Vol 3] Part G, Section 3.3.3.5 [4]. 

##### **2.4.1 GATT Format Types** 

Referenced from Bluetooth Core Specification [Vol 3] Part G, Section 3.3.3.5.2 [4]. 

Last Modified: 2024-02-05 

Filename: assigned_numbers/core/formattypes.yaml 

|**Value**|**Type Name**|**Description**|
|---|---|---|
|0x01|boolean|unsigned 1-bit; 0 = false; 1 = true|
|0x02|uint2|unsigned 2-bit integer|
|0x03|uint4|unsigned 4-bit integer|
|0x04|uint8|unsigned 8-bit integer|
|0x05|uint12|unsigned 12-bit integer|
|0x06|uint16|unsigned 16-bit integer|
|0x07|uint24|unsigned 24-bit integer|
|0x08|uint32|unsigned 32-bit integer|
|0x09|uint48|unsigned 48-bit integer|
|0x0A|uint64|unsigned 64-bit integer|
|0x0B|uint128|unsigned 128-bit integer|
|0x0C|sint8|signed 8-bit integer|
|0x0D|sint12|signed 12-bit integer|
|0x0E|sint16|signed 16-bit integer|
|0x0F|sint24|signed 24-bit integer|
|0x10|sint32|signed 32-bit integer|
|0x11|sint48|signed 48-bit integer|
|0x12|sint64|signed 64-bit integer|
|0x13|sint128|signed 128-bit integer|
|0x14|float32|IEEE-754 32-bit floating point|
|0x15|float64|IEEE-754 64-bit floating point|
|0x16|medfloat16|IEEE 11073-20601 16-bit SFLOAT|
|0x17|medfloat32|IEEE 11073-20601 32-bit FLOAT|
|0x18|uint16[2]|IEEE 11073-20601 nomenclature code|
|0x19|utf8s|UTF-8 string|
|0x1A|utf16s|UTF-16 string|
|0x1B|struct|opaque structure|



Bluetooth SIG Proprietary 

Page 15 of 442 

**Assigned Numbers** / Document 

**2026-06-23** 

|0x1C|medASN1|IEEE-11073 ASN.1/MDER structure|
|---|---|---|



Bluetooth SIG Proprietary 

Page 16 of 442 

**Assigned Numbers** / Document 

**2026-06-23** 

##### **2.4.2 GATT Characteristic Presentation Format Name Space** 

Referenced from Bluetooth Core Specification [Vol 3] Part G, Section 3.3.3.5.5 [4]. 

Last Modified: 2024-02-05 

Filename: assigned_numbers/core/namespaces.yaml 

|**Value**|**Name**|**Reference**|
|---|---|---|
|0x01|Bluetooth SIG|Section 2.4.2.1|



Bluetooth SIG Proprietary 

Page 17 of 442 

**Assigned Numbers** / Document 

**2026-06-23** 

###### **2.4.2.1 Bluetooth SIG GATT Characteristic Presentation Format Description** 

Referenced from Bluetooth Core Specification [Vol 3] Part G, Section 3.3.3.5.6 [4]. 

Last Modified: 2024-02-05 

Filename: assigned_numbers/core/namespace.yaml 

|**Value**|**Name**|
|---|---|
|0x0000|unknown|
|0x0001|first|
|0x0002|second|
|0x0003|third|
|0x0004|fourth|
|0x0005|fifth|
|0x0006|sixth|
|0x0007|seventh|
|0x0008|eighth|
|0x0009|ninth|
|0x000A|tenth|
|0x000B|eleventh|
|0x000C|twelfth|
|0x000D|thirteenth|
|0x000E|fourteenth|
|0x000F|fifteenth|
|0x0010|sixteenth|
|0x0011|seventeenth|
|0x0012|eighteenth|
|0x0013|nineteenth|
|0x0014|twentieth|
|0x0015|twenty-first|
|0x0016|twenty-second|
|0x0017|twenty-third|
|0x0018|twenty-fourth|
|0x0019|twenty-fifth|
|0x001A|twenty-sixth|
|0x001B|twenty-seventh|
|0x001C|twenty-eighth|
|0x001D|twenty-ninth|



Bluetooth SIG Proprietary 

Page 18 of 442 

**Assigned Numbers** / Document 

**2026-06-23** 

|0x001E|thirtieth|
|---|---|
|0x001F|thirty-first|
|0x0020|thirty-second|
|0x0021|thirty-third|
|0x0022|thirty-fourth|
|0x0023|thirty-fifth|
|0x0024|thirty-sixth|
|0x0025|thirty-seventh|
|0x0026|thirty-eighth|
|0x0027|thirty-ninth|
|0x0028|fortieth|
|0x0029|forty-first|
|0x002A|forty-second|
|0x002B|forty-third|
|0x002C|forty-fourth|
|0x002D|forty-fifth|
|0x002E|forty-sixth|
|0x002F|forty-seventh|
|0x0030|forty-eighth|
|0x0031|forty-ninth|
|0x0032|fiftieth|
|0x0033|fifty-first|
|0x0034|fifty-second|
|0x0035|fifty-third|
|0x0036|fifty-fourth|
|0x0037|fifty-fifth|
|0x0038|fifty-sixth|
|0x0039|fifty-seventh|
|0x003A|fifty-eighth|
|0x003B|fifty-ninth|
|0x003C|sixtieth|
|0x003D|sixty-first|
|0x003E|sixty-second|
|0x003F|sixty-third|
|0x0040|sixty-fourth|
|0x0041|sixty-fifth|



Bluetooth SIG Proprietary 

Page 19 of 442 

**Assigned Numbers** / Document 

**2026-06-23** 

|0x0042|sixty-sixth|
|---|---|
|0x0043|sixty-seventh|
|0x0044|sixty-eighth|
|0x0045|sixty-ninth|
|0x0046|seventieth|
|0x0047|seventy-first|
|0x0048|seventy-second|
|0x0049|seventy-third|
|0x004A|seventy-fourth|
|0x004B|seventy-fifth|
|0x004C|seventy-sixth|
|0x004D|seventy-seventh|
|0x004E|seventy-eighth|
|0x004F|seventy-ninth|
|0x0050|eightieth|
|0x0051|eighty-first|
|0x0052|eighty-second|
|0x0053|eighty-third|
|0x0054|eighty-fourth|
|0x0055|eighty-fifth|
|0x0056|eighty-sixth|
|0x0057|eighty-seventh|
|0x0058|eighty-eighth|
|0x0059|eighty-ninth|
|0x005A|ninetieth|
|0x005B|ninety-first|
|0x005C|ninety-second|
|0x005D|ninety-third|
|0x005E|ninety-fourth|
|0x005F|ninety-fifth|
|0x0060|ninety-sixth|
|0x0061|ninety-seventh|
|0x0062|ninety-eighth|
|0x0063|ninety-ninth|
|0x0064|one-hundredth|
|0x0065|one-hundred-and-first|



Bluetooth SIG Proprietary 

Page 20 of 442 

**Assigned Numbers** / Document 

**2026-06-23** 

|0x0066|one-hundred-and-second|
|---|---|
|0x0067|one-hundred-and-third|
|0x0068|one-hundred-and-fourth|
|0x0069|one-hundred-and-fifth|
|0x006A|one-hundred-and-sixth|
|0x006B|one-hundred-and-seventh|
|0x006C|one-hundred-and-eighth|
|0x006D|one-hundred-and-ninth|
|0x006E|one-hundred-and-tenth|
|0x006F|one-hundred-and-eleventh|
|0x0070|one-hundred-and-twelfth|
|0x0071|one-hundred-and-thirteenth|
|0x0072|one-hundred-and-fourteenth|
|0x0073|one-hundred-and-fifteenth|
|0x0074|one-hundred-and-sixteenth|
|0x0075|one-hundred-and-seventeenth|
|0x0076|one-hundred-and-eighteenth|
|0x0077|one-hundred-and-nineteenth|
|0x0078|one-hundred-twentieth|
|0x0079|one-hundred-and-twenty-first|
|0x007A|one-hundred-and-twenty-second|
|0x007B|one-hundred-and-twenty-third|
|0x007C|one-hundred-and-twenty-fourth|
|0x007D|one-hundred-and-twenty-fifth|
|0x007E|one-hundred-and-twenty-sixth|
|0x007F|one-hundred-and-twenty-seventh|
|0x0080|one-hundred-and-twenty-eighth|
|0x0081|one-hundred-and-twenty-ninth|
|0x0082|one-hundred-thirtieth|
|0x0083|one-hundred-and-thirty-first|
|0x0084|one-hundred-and-thirty-second|
|0x0085|one-hundred-and-thirty-third|
|0x0086|one-hundred-and-thirty-fourth|
|0x0087|one-hundred-and-thirty-fifth|
|0x0088|one-hundred-and-thirty-sixth|
|0x0089|one-hundred-and-thirty-seventh|



Bluetooth SIG Proprietary 

Page 21 of 442 

**Assigned Numbers** / Document 

**2026-06-23** 

|0x008A|one-hundred-and-thirty-eighth|
|---|---|
|0x008B|one-hundred-and-thirty-ninth|
|0x008C|one-hundred-fortieth|
|0x008D|one-hundred-and-forty-first|
|0x008E|one-hundred-and-forty-second|
|0x008F|one-hundred-and-forty-third|
|0x0090|one-hundred-and-forty-fourth|
|0x0091|one-hundred-and-forty-fifth|
|0x0092|one-hundred-and-forty-sixth|
|0x0093|one-hundred-and-forty-seventh|
|0x0094|one-hundred-and-forty-eighth|
|0x0095|one-hundred-and-forty-ninth|
|0x0096|one-hundred-fiftieth|
|0x0097|one-hundred-and-fifty-first|
|0x0098|one-hundred-and-fifty-second|
|0x0099|one-hundred-and-fifty-third|
|0x009A|one-hundred-and-fifty-fourth|
|0x009B|one-hundred-and-fifty-fifth|
|0x009C|one-hundred-and-fifty-sixth|
|0x009D|one-hundred-and-fifty-seventh|
|0x009E|one-hundred-and-fifty-eighth|
|0x009F|one-hundred-and-fifty-ninth|
|0x00A0|one-hundred-sixtieth|
|0x00A1|one-hundred-and-sixty-first|
|0x00A2|one-hundred-and-sixty-second|
|0x00A3|one-hundred-and-sixty-third|
|0x00A4|one-hundred-and-sixty-fourth|
|0x00A5|one-hundred-and-sixty-fifth|
|0x00A6|one-hundred-and-sixty-sixth|
|0x00A7|one-hundred-and-sixty-seventh|
|0x00A8|one-hundred-and-sixty-eighth|
|0x00A9|one-hundred-and-sixty-ninth|
|0x00AA|one-hundred-seventieth|
|0x00AB|one-hundred-and-seventy-first|
|0x00AC|one-hundred-and-seventy-second|
|0x00AD|one-hundred-and-seventy-third|



Bluetooth SIG Proprietary 

Page 22 of 442 

**Assigned Numbers** / Document 

**2026-06-23** 

|0x00AE|one-hundred-and-seventy-fourth|
|---|---|
|0x00AF|one-hundred-and-seventy-fifth|
|0x00B0|one-hundred-and-seventy-sixth|
|0x00B1|one-hundred-and-seventy-seventh|
|0x00B2|one-hundred-and-seventy-eighth|
|0x00B3|one-hundred-and-seventy-ninth|
|0x00B4|one-hundred-eightieth|
|0x00B5|one-hundred-and-eighty-first|
|0x00B6|one-hundred-and-eighty-second|
|0x00B7|one-hundred-and-eighty-third|
|0x00B8|one-hundred-and-eighty-fourth|
|0x00B9|one-hundred-and-eighty-fifth|
|0x00BA|one-hundred-and-eighty-sixth|
|0x00BB|one-hundred-and-eighty-seventh|
|0x00BC|one-hundred-and-eighty-eighth|
|0x00BD|one-hundred-and-eighty-ninth|
|0x00BE|one-hundred-ninetieth|
|0x00BF|one-hundred-and-ninety-first|
|0x00C0|one-hundred-and-ninety-second|
|0x00C1|one-hundred-and-ninety-third|
|0x00C2|one-hundred-and-ninety-fourth|
|0x00C3|one-hundred-and-ninety-fifth|
|0x00C4|one-hundred-and-ninety-sixth|
|0x00C5|one-hundred-and-ninety-seventh|
|0x00C6|one-hundred-and-ninety-eighth|
|0x00C7|one-hundred-and-ninety-ninth|
|0x00C8|two-hundredth|
|0x00C9|two-hundred-and-first|
|0x00CA|two-hundred-and-second|
|0x00CB|two-hundred-and-third|
|0x00CC|two-hundred-and-fourth|
|0x00CD|two-hundred-and-fifth|
|0x00CE|two-hundred-and-sixth|
|0x00CF|two-hundred-and-seventh|
|0x00D0|two-hundred-and-eighth|
|0x00D1|two-hundred-and-ninth|



Bluetooth SIG Proprietary 

Page 23 of 442 

**Assigned Numbers** / Document 

**2026-06-23** 

|0x00D2|two-hundred-and-tenth|
|---|---|
|0x00D3|two-hundred-and-eleventh|
|0x00D4|two-hundred-and-twelfth|
|0x00D5|two-hundred-and-thirteenth|
|0x00D6|two-hundred-and-fourteenth|
|0x00D7|two-hundred-and-fifteenth|
|0x00D8|two-hundred-and-sixteenth|
|0x00D9|two-hundred-and-seventeenth|
|0x00DA|two-hundred-and-eighteenth|
|0x00DB|two-hundred-and-nineteenth|
|0x00DC|two-hundred-twentieth|
|0x00DD|two-hundred-and-twenty-first|
|0x00DE|two-hundred-and-twenty-second|
|0x00DF|two-hundred-and-twenty-third|
|0x00E0|two-hundred-and-twenty-fourth|
|0x00E1|two-hundred-and-twenty-fifth|
|0x00E2|two-hundred-and-twenty-sixth|
|0x00E3|two-hundred-and-twenty-seventh|
|0x00E4|two-hundred-and-twenty-eighth|
|0x00E5|two-hundred-and-twenty-ninth|
|0x00E6|two-hundred-thirtieth|
|0x00E7|two-hundred-and-thirty-first|
|0x00E8|two-hundred-and-thirty-second|
|0x00E9|two-hundred-and-thirty-third|
|0x00EA|two-hundred-and-thirty-fourth|
|0x00EB|two-hundred-and-thirty-fifth|
|0x00EC|two-hundred-and-thirty-sixth|
|0x00ED|two-hundred-and-thirty-seventh|
|0x00EE|two-hundred-and-thirty-eighth|
|0x00EF|two-hundred-and-thirty-ninth|
|0x00F0|two-hundred-fortieth|
|0x00F1|two-hundred-and-forty-first|
|0x00F2|two-hundred-and-forty-second|
|0x00F3|two-hundred-and-forty-third|
|0x00F4|two-hundred-and-forty-fourth|
|0x00F5|two-hundred-and-forty-fifth|



Bluetooth SIG Proprietary 

Page 24 of 442 

**Assigned Numbers** / Document 

**2026-06-23** 

|0x00F6|two-hundred-and-forty-sixth|
|---|---|
|0x00F7|two-hundred-and-forty-seventh|
|0x00F8|two-hundred-and-forty-eighth|
|0x00F9|two-hundred-and-forty-ninth|
|0x00FA|two-hundred-fiftieth|
|0x00FB|two-hundred-and-fifty-first|
|0x00FC|two-hundred-and-fifty-second|
|0x00FD|two-hundred-and-fifty-third|
|0x00FE|two-hundred-and-fifty-fourth|
|0x00FF|two-hundred-and-fifty-fifth|
|0x0100|front|
|0x0101|back|
|0x0102|top|
|0x0103|bottom|
|0x0104|upper|
|0x0105|lower|
|0x0106|main|
|0x0107|backup|
|0x0108|auxiliary|
|0x0109|supplementary|
|0x010A|flash|
|0x010B|inside|
|0x010C|outside|
|0x010D|left|
|0x010E|right|
|0x010F|internal|
|0x0110|external|



Bluetooth SIG Proprietary 

Page 25 of 442 

**Assigned Numbers** / Document 

**2026-06-23** 

#### **2.5 PSMs and SPSMs** 

Referenced from the following: 

- Bluetooth Core Specification [Vol 3] Part A, Section 4.2 [4]. 

- Bluetooth Core Specification [Vol 3] Part A, Section 4.22 [4]. 

Last Modified: 2024-02-05 

Filename: assigned_numbers/core/psm.yaml 

|**Value**|**Name**|**Reference**|**Type**|
|---|---|---|---|
|0x0001|SDP|Bluetooth Core Specification [4]|PSM|
|0x0003|RFCOMM|RFCOMM [21]|PSM|
|0x0005|TCS-BIN|Telephony Control Protocol [23]|PSM|
|0x0007|TCS-BIN-CORDLESS|Telephony Control Protocol [23]|PSM|
|0x000F|BNEP|Bluetooth Network Encapsulation<br>Protocol [5]|PSM|
|0x0011|HID_Control|Human Interface Device Profile [11]|PSM|
|0x0013|HID_Interrupt|Human Interface Device Profile [11]|PSM|
|0x0015|UPnP|Extended Service Discovery Profile for<br>UPnP [8]|PSM|
|0x0017|AVCTP|Audio/Video Control Transport Protocol<br>[2]|PSM|
|0x0019|AVDTP|Audio/Video Distribution Transport<br>Protocol [3]|PSM|
|0x001B|AVCTP_Browsing|Audio/Video Control Transport Protocol<br>[2]|PSM|
|0x001D|UDI_C-Plane|Unrestricted Digital Information Profile<br>[24]|PSM|
|0x001F|ATT|Bluetooth Core Specification [4]|PSM|
|0x0021|3DSP|3D Synchronization Profile [1]|PSM|
|0x0023|LE_PSM_IPSP|Internet Protocol Support Profile [12]|SPSM|
|0x0025|OTS|Object Transfer Service [19]|PSM or<br>SPSM|
|0x0027|EATT|Bluetooth Core Specification [4]|PSM or<br>SPSM|



Bluetooth SIG Proprietary 

Page 26 of 442 

**Assigned Numbers** / Document 

**2026-06-23** 

#### **2.6 Appearance Values** 

Referenced from the following: 

- Bluetooth Core Specification [Vol 3] Part C, Section 12.2 [4]. 

- Supplement to the Bluetooth Core Specification Part A, Section 1.12.2 [22]. 

##### **2.6.1 Definition** 



<!-- Start of picture text -->
15 6 5 0<br>Category, see Section 2.6.2 Sub-category, see Section 2.6.3<br><!-- End of picture text -->

_Figure 2.1: Appearance Value format_ 

Bluetooth SIG Proprietary 

Page 27 of 442 

**Assigned Numbers** / Document 

**2026-06-23** 

##### **2.6.2 Appearance Category ranges** 

These are the assigned ranges of the appearance categories. 

###### Last Modified: 2025-11-04 

Filename: assigned_numbers/core/appearance_values.yaml 

|**Category (bits 15 to 6)**|**Value Ranges**|**Name**|
|---|---|---|
|0x000|0x0000to0x003F|Unknown|
|0x001|0x0040to0x007F|Phone|
|0x002|0x0080to0x00BF|Computer|
|0x003|0x00C0to0x00FF|Watch|
|0x004|0x0100to0x013F|Clock|
|0x005|0x0140to0x017F|Display|
|0x006|0x0180to0x01BF|Remote Control|
|0x007|0x01C0to0x01FF|Eye-glasses|
|0x008|0x0200to0x023F|Tag|
|0x009|0x0240to0x027F|Keyring|
|0x00A|0x0280to0x02BF|Media Player|
|0x00B|0x02C0to0x02FF|Barcode Scanner|
|0x00C|0x0300to0x033F|Thermometer|
|0x00D|0x0340to0x037F|Heart Rate Sensor|
|0x00E|0x0380to0x03BF|Blood Pressure|
|0x00F|0x03C0to0x03FF|Human Interface Device|
|0x010|0x0400to0x043F|Glucose Meter|
|0x011|0x0440to0x047F|Running Walking Sensor|
|0x012|0x0480to0x04BF|Cycling|
|0x013|0x04C0to0x04FF|Control Device|
|0x014|0x0500to0x053F|Network Device|
|0x015|0x0540to0x057F|Sensor|
|0x016|0x0580to0x05BF|Light Fixtures|
|0x017|0x05C0to0x05FF|Fan|
|0x018|0x0600to0x063F|HVAC|
|0x019|0x0640to0x067F|Air Conditioning|
|0x01A|0x0680to0x06BF|Humidifier|
|0x01B|0x06C0to0x06FF|Heating|
|0x01C|0x0700to0x073F|Access Control|
|0x01D|0x0740to0x077F|Motorized Device|



Bluetooth SIG Proprietary 

Page 28 of 442 

**Assigned Numbers** / Document 

**2026-06-23** 

|0x01E|0x0780to0x07BF|Power Device|
|---|---|---|
|0x01F|0x07C0to0x07FF|Light Source|
|0x020|0x0800to0x083F|Window Covering|
|0x021|0x0840to0x087F|Audio Sink|
|0x022|0x0880to0x08BF|Audio Source|
|0x023|0x08C0to0x08FF|Motorized Vehicle|
|0x024|0x0900to0x093F|Domestic Appliance|
|0x025|0x0940to0x097F|Wearable Audio Device|
|0x026|0x0980to0x09BF|Aircraft|
|0x027|0x09C0to0x09FF|AV Equipment|
|0x028|0x0A00to0x0A3F|Display Equipment|
|0x029|0x0A40to0x0A7F|Hearing aid|
|0x02A|0x0A80to0x0ABF|Gaming|
|0x02B|0x0AC0to0x0AFF|Signage|
|0x031|0x0C40to0x0C7F|Pulse Oximeter|
|0x032|0x0C80to0x0CBF|Weight Scale|
|0x033|0x0CC0to0x0CFF|Personal Mobility Device|
|0x034|0x0D00to0x0D3F|Continuous Glucose Monitor|
|0x035|0x0D40to0x0D7F|Insulin Pump|
|0x036|0x0D80to0x0DBF|Medication Delivery|
|0x037|0x0DC0to0x0DFF|Spirometer|
|0x051|0x1440to0x147F|Outdoor Sports Activity|
|0x052|0x1480to0x14BF|Industrial Measurement Device|
|0x053|0x14C0to0x14FF|Industrial Tools|
|0x054|0x1500to0x153F|Cookware Device|



Bluetooth SIG Proprietary 

Page 29 of 442 

**Assigned Numbers** / Document 

**2026-06-23** 

##### **2.6.3 Appearance Sub-category values** 

These are the assigned values within the assigned ranges of appearance categories. Last Modified: 2025-11-04 

Filename: assigned_numbers/core/appearance_values.yaml 

|**Category**<br>**(bits 15 to 6)**|**Subcatgeory**<br>**(bits 5 to 0)**|**Value**|**Name**|
|---|---|---|---|
|0x000|0x00|0x0000|Generic Unknown|
|0x001|0x00|0x0040|Generic Phone|
|0x002|0x00|0x0080|Generic Computer|
||0x01|0x0081|Desktop Workstation|
||0x02|0x0082|Server-class Computer|
||0x03|0x0083|Laptop|
||0x04|0x0084|Handheld PC/PDA (clamshell)|
||0x05|0x0085|Palm-size PC/PDA|
||0x06|0x0086|Wearable computer (watch size)|
||0x07|0x0087|Tablet|
||0x08|0x0088|Docking Station|
||0x09|0x0089|All in One|
||0x0A|0x008A|Blade Server|
||0x0B|0x008B|Convertible|
||0x0C|0x008C|Detachable|
||0x0D|0x008D|IoT Gateway|
||0x0E|0x008E|Mini PC|
||0x0F|0x008F|Stick PC|
|0x003|0x00|0x00C0|Generic Watch|
||0x01|0x00C1|Sports Watch|
||0x02|0x00C2|Smartwatch|
|0x004|0x00|0x0100|Generic Clock|
|0x005|0x00|0x0140|Generic Display|
|0x006|0x00|0x0180|Generic Remote Control|
|0x007|0x00|0x01C0|Generic Eye-glasses|
|0x008|0x00|0x0200|Generic Tag|
|0x009|0x00|0x0240|Generic Keyring|
|0x00A|0x00|0x0280|Generic Media Player|
|0x00B|0x00|0x02C0|Generic Barcode Scanner|
|0x00C|0x00|0x0300|Generic Thermometer|



Bluetooth SIG Proprietary 

Page 30 of 442 

**Assigned Numbers** / Document 

**2026-06-23** 

||0x01|0x0301|Ear Thermometer|
|---|---|---|---|
|0x00D|0x00|0x0340|Generic Heart Rate Sensor|
||0x01|0x0341|Heart Rate Belt|
|0x00E|0x00|0x0380|Generic Blood Pressure|
||0x01|0x0381|Arm Blood Pressure|
||0x02|0x0382|Wrist Blood Pressure|
|0x00F|0x00|0x03C0|Generic Human Interface Device|
||0x01|0x03C1|Keyboard|
||0x02|0x03C2|Mouse|
||0x03|0x03C3|Joystick|
||0x04|0x03C4|Gamepad|
||0x05|0x03C5|Digitizer Tablet|
||0x06|0x03C6|Card Reader|
||0x07|0x03C7|Digital Pen|
||0x08|0x03C8|Barcode Scanner|
||0x09|0x03C9|Touchpad|
||0x0A|0x03CA|Presentation Remote|
|0x010|0x00|0x0400|Generic Glucose Meter|
|0x011|0x00|0x0440|Generic Running Walking Sensor|
||0x01|0x0441|In-Shoe Running Walking Sensor|
||0x02|0x0442|On-Shoe Running Walking Sensor|
||0x03|0x0443|On-Hip Running Walking Sensor|
|0x012|0x00|0x0480|Generic Cycling|
||0x01|0x0481|Cycling Computer|
||0x02|0x0482|Speed Sensor|
||0x03|0x0483|Cadence Sensor|
||0x04|0x0484|Power Sensor|
||0x05|0x0485|Speed and Cadence Sensor|
|0x013|0x00|0x04C0|Generic Control Device|
||0x01|0x04C1|Switch|
||0x02|0x04C2|Multi-switch|
||0x03|0x04C3|Button|
||0x04|0x04C4|Slider|
||0x05|0x04C5|Rotary Switch|
||0x06|0x04C6|Touch Panel|
||0x07|0x04C7|Single Switch|
|Bluetooth SIG|0x08<br>Proprietary|0x04C8|Double Switch<br>Pa|



Page 31 of 442 

**Assigned Numbers** / Document 

**2026-06-23** 

||0x09|0x04C9|Triple Switch|
|---|---|---|---|
||0x0A|0x04CA|Battery Switch|
||0x0B|0x04CB|Energy Harvesting Switch|
||0x0C|0x04CC|Push Button|
||0x0D|0x04CD|Dial|
|0x014|0x00|0x0500|Generic Network Device|
||0x01|0x0501|Access Point|
||0x02|0x0502|Mesh Device|
||0x03|0x0503|Mesh Network Proxy|
|0x015|0x00|0x0540|Generic Sensor|
||0x01|0x0541|Motion Sensor|
||0x02|0x0542|Air quality Sensor|
||0x03|0x0543|Temperature Sensor|
||0x04|0x0544|Humidity Sensor|
||0x05|0x0545|Leak Sensor|
||0x06|0x0546|Smoke Sensor|
||0x07|0x0547|Occupancy Sensor|
||0x08|0x0548|Contact Sensor|
||0x09|0x0549|Carbon Monoxide Sensor|
||0x0A|0x054A|Carbon Dioxide Sensor|
||0x0B|0x054B|Ambient Light Sensor|
||0x0C|0x054C|Energy Sensor|
||0x0D|0x054D|Color Light Sensor|
||0x0E|0x054E|Rain Sensor|
||0x0F|0x054F|Fire Sensor|
||0x10|0x0550|Wind Sensor|
||0x11|0x0551|Proximity Sensor|
||0x12|0x0552|Multi-Sensor|
||0x13|0x0553|Flush Mounted Sensor|
||0x14|0x0554|Ceiling Mounted Sensor|
||0x15|0x0555|Wall Mounted Sensor|
||0x16|0x0556|Multisensor|
||0x17|0x0557|Energy Meter|
||0x18|0x0558|Flame Detector|
||0x19|0x0559|Vehicle Tire Pressure Sensor|
|0x016|0x00|0x0580|Generic Light Fixtures|
|Bluetooth SIG|0x01<br>Proprietary|0x0581|Wall Light|



Page 32 of 442 

**Assigned Numbers** / Document 

**2026-06-23** 

||0x02|0x0582|Ceiling Light|
|---|---|---|---|
||0x03|0x0583|Floor Light|
||0x04|0x0584|Cabinet Light|
||0x05|0x0585|Desk Light|
||0x06|0x0586|Troffer Light|
||0x07|0x0587|Pendant Light|
||0x08|0x0588|In-ground Light|
||0x09|0x0589|Flood Light|
||0x0A|0x058A|Underwater Light|
||0x0B|0x058B|Bollard with Light|
||0x0C|0x058C|Pathway Light|
||0x0D|0x058D|Garden Light|
||0x0E|0x058E|Pole-top Light|
||0x0F|0x058F|Spotlight|
||0x10|0x0590|Linear Light|
||0x11|0x0591|Street Light|
||0x12|0x0592|Shelves Light|
||0x13|0x0593|Bay Light|
||0x14|0x0594|Emergency Exit Light|
||0x15|0x0595|Light Controller|
||0x16|0x0596|Light Driver|
||0x17|0x0597|Bulb|
||0x18|0x0598|Low-bay Light|
||0x19|0x0599|High-bay Light|
|0x017|0x00|0x05C0|Generic Fan|
||0x01|0x05C1|Ceiling Fan|
||0x02|0x05C2|Axial Fan|
||0x03|0x05C3|Exhaust Fan|
||0x04|0x05C4|Pedestal Fan|
||0x05|0x05C5|Desk Fan|
||0x06|0x05C6|Wall Fan|
|0x018|0x00|0x0600|Generic HVAC|
||0x01|0x0601|Thermostat|
||0x02|0x0602|Humidifier|
||0x03|0x0603|De-humidifier|
||0x04|0x0604|Heater|
||0x05|0x0605|Radiator|



Bluetooth SIG Proprietary 

Page 33 of 442 

**Assigned Numbers** / Document 

**2026-06-23** 

||0x06|0x0606|Boiler|
|---|---|---|---|
||0x07|0x0607|Heat Pump|
||0x08|0x0608|Infrared Heater|
||0x09|0x0609|Radiant Panel Heater|
||0x0A|0x060A|Fan Heater|
||0x0B|0x060B|Air Curtain|
|0x019|0x00|0x0640|Generic Air Conditioning|
|0x01A|0x00|0x0680|Generic Humidifier|
|0x01B|0x00|0x06C0|Generic Heating|
||0x01|0x06C1|Radiator|
||0x02|0x06C2|Boiler|
||0x03|0x06C3|Heat Pump|
||0x04|0x06C4|Infrared Heater|
||0x05|0x06C5|Radiant Panel Heater|
||0x06|0x06C6|Fan Heater|
||0x07|0x06C7|Air Curtain|
|0x01C|0x00|0x0700|Generic Access Control|
||0x01|0x0701|Access Door|
||0x02|0x0702|Garage Door|
||0x03|0x0703|Emergency Exit Door|
||0x04|0x0704|Access Lock|
||0x05|0x0705|Elevator|
||0x06|0x0706|Window|
||0x07|0x0707|Entrance Gate|
||0x08|0x0708|Door Lock|
||0x09|0x0709|Locker|
|0x01D|0x00|0x0740|Generic Motorized Device|
||0x01|0x0741|Motorized Gate|
||0x02|0x0742|Awning|
||0x03|0x0743|Blinds or Shades|
||0x04|0x0744|Curtains|
||0x05|0x0745|Screen|
|0x01E|0x00|0x0780|Generic Power Device|
||0x01|0x0781|Power Outlet|
||0x02|0x0782|Power Strip|
||0x03|0x0783|Plug|
||0x04|0x0784|Power Supply|



Bluetooth SIG Proprietary 

Page 34 of 442 

**Assigned Numbers** / Document 

**2026-06-23** 

||0x05|0x0785|LED Driver|
|---|---|---|---|
||0x06|0x0786|Fluorescent Lamp Gear|
||0x07|0x0787|HID Lamp Gear|
||0x08|0x0788|Charge Case|
||0x09|0x0789|Power Bank|
|0x01F|0x00|0x07C0|Generic Light Source|
||0x01|0x07C1|Incandescent Light Bulb|
||0x02|0x07C2|LED Lamp|
||0x03|0x07C3|HID Lamp|
||0x04|0x07C4|Fluorescent Lamp|
||0x05|0x07C5|LED Array|
||0x06|0x07C6|Multi-Color LED Array|
||0x07|0x07C7|Low voltage halogen|
||0x08|0x07C8|Organic light emitting diode (OLED)|
|0x020|0x00|0x0800|Generic Window Covering|
||0x01|0x0801|Window Shades|
||0x02|0x0802|Window Blinds|
||0x03|0x0803|Window Awning|
||0x04|0x0804|Window Curtain|
||0x05|0x0805|Exterior Shutter|
||0x06|0x0806|Exterior Screen|
|0x021|0x00|0x0840|Generic Audio Sink|
||0x01|0x0841|Standalone Speaker|
||0x02|0x0842|Soundbar|
||0x03|0x0843|Bookshelf Speaker|
||0x04|0x0844|Standmounted Speaker|
||0x05|0x0845|Speakerphone|
|0x022|0x00|0x0880|Generic Audio Source|
||0x01|0x0881|Microphone|
||0x02|0x0882|Alarm|
||0x03|0x0883|Bell|
||0x04|0x0884|Horn|
||0x05|0x0885|Broadcasting Device|
||0x06|0x0886|Service Desk|
||0x07|0x0887|Kiosk|
||0x08|0x0888|Broadcasting Room|



Bluetooth SIG Proprietary 

Page 35 of 442 

**Assigned Numbers** / Document 

**2026-06-23** 

||0x09|0x0889|Auditorium|
|---|---|---|---|
|0x023|0x00|0x08C0|Generic Motorized Vehicle|
||0x01|0x08C1|Car|
||0x02|0x08C2|Large Goods Vehicle|
||0x03|0x08C3|2-Wheeled Vehicle|
||0x04|0x08C4|Motorbike|
||0x05|0x08C5|Scooter|
||0x06|0x08C6|Moped|
||0x07|0x08C7|3-Wheeled Vehicle|
||0x08|0x08C8|Light Vehicle|
||0x09|0x08C9|Quad Bike|
||0x0A|0x08CA|Minibus|
||0x0B|0x08CB|Bus|
||0x0C|0x08CC|Trolley|
||0x0D|0x08CD|Agricultural Vehicle|
||0x0E|0x08CE|Camper / Caravan|
||0x0F|0x08CF|Recreational Vehicle / Motor Home|
|0x024|0x00|0x0900|Generic Domestic Appliance|
||0x01|0x0901|Refrigerator|
||0x02|0x0902|Freezer|
||0x03|0x0903|Oven|
||0x04|0x0904|Microwave|
||0x05|0x0905|Toaster|
||0x06|0x0906|Washing Machine|
||0x07|0x0907|Dryer|
||0x08|0x0908|Coffee maker|
||0x09|0x0909|Clothes iron|
||0x0A|0x090A|Curling iron|
||0x0B|0x090B|Hair dryer|
||0x0C|0x090C|Vacuum cleaner|
||0x0D|0x090D|Robotic vacuum cleaner|
||0x0E|0x090E|Rice cooker|
||0x0F|0x090F|Clothes steamer|
|0x025|0x00|0x0940|Generic Wearable Audio Device|
||0x01|0x0941|Earbud|
||0x02|0x0942|Headset|
|Bluetooth SIG|0x03<br>Proprietary|0x0943|Headphones<br>Pag|



Page 36 of 442 

**Assigned Numbers** / Document 

**2026-06-23** 

||0x04|0x0944|Neck Band|
|---|---|---|---|
||0x05|0x0945|Left Earbud|
||0x06|0x0946|Right Earbud|
|0x026|0x00|0x0980|Generic Aircraft|
||0x01|0x0981|Light Aircraft|
||0x02|0x0982|Microlight|
||0x03|0x0983|Paraglider|
||0x04|0x0984|Large Passenger Aircraft|
|0x027|0x00|0x09C0|Generic AV Equipment|
||0x01|0x09C1|Amplifier|
||0x02|0x09C2|Receiver|
||0x03|0x09C3|Radio|
||0x04|0x09C4|Tuner|
||0x05|0x09C5|Turntable|
||0x06|0x09C6|CD Player|
||0x07|0x09C7|DVD Player|
||0x08|0x09C8|Bluray Player|
||0x09|0x09C9|Optical Disc Player|
||0x0A|0x09CA|Set-Top Box|
|0x028|0x00|0x0A00|Generic Display Equipment|
||0x01|0x0A01|Television|
||0x02|0x0A02|Monitor|
||0x03|0x0A03|Projector|
|0x029|0x00|0x0A40|Generic Hearing aid|
||0x01|0x0A41|In-ear hearing aid|
||0x02|0x0A42|Behind-ear hearing aid|
||0x03|0x0A43|Cochlear Implant|
|0x02A|0x00|0x0A80|Generic Gaming|
||0x01|0x0A81|Home Video Game Console|
||0x02|0x0A82|Portable handheld console|
|0x02B|0x00|0x0AC0|Generic Signage|
||0x01|0x0AC1|Digital Signage|
||0x02|0x0AC2|Electronic Label|
|0x031|0x00|0x0C40|Generic Pulse Oximeter|
||0x01|0x0C41|Fingertip Pulse Oximeter|
||0x02|0x0C42|Wrist Worn Pulse Oximeter|



Bluetooth SIG Proprietary 

Page 37 of 442 

**Assigned Numbers** / Document 

**2026-06-23** 

|0x032|0x00|0x0C80|Generic Weight Scale|
|---|---|---|---|
|0x033|0x00|0x0CC0|Generic Personal Mobility Device|
||0x01|0x0CC1|Powered Wheelchair|
||0x02|0x0CC2|Mobility Scooter|
|0x034|0x00|0x0D00|Generic Continuous Glucose Monitor|
|0x035|0x00|0x0D40|Generic Insulin Pump|
||0x01|0x0D41|Insulin Pump, durable pump|
||0x04|0x0D44|Insulin Pump, patch pump|
||0x08|0x0D48|Insulin Pen|
|0x036|0x00|0x0D80|Generic Medication Delivery|
|0x037|0x00|0x0DC0|Generic Spirometer|
||0x01|0x0DC1|Handheld Spirometer|
|0x051|0x00|0x1440|Generic Outdoor Sports Activity|
||0x01|0x1441|Location Display|
||0x02|0x1442|Location and Navigation Display|
||0x03|0x1443|Location Pod|
||0x04|0x1444|Location and Navigation Pod|
|0x052|0x00|0x1480|Generic Industrial Measurement Device|
||0x01|0x1481|Torque Testing Device|
||0x02|0x1482|Caliper|
||0x03|0x1483|Dial Indicator|
||0x04|0x1484|Micrometer|
||0x05|0x1485|Height Gauge|
||0x06|0x1486|Force Gauge|
|0x053|0x00|0x14C0|Generic Industrial Tools|
||0x01|0x14C1|Machine Tool Holder|
||0x02|0x14C2|Generic Clamping Device|
||0x03|0x14C3|Clamping Jaws/Jaw Chuck|
||0x04|0x14C4|Clamping (Collet) Chuck|
||0x05|0x14C5|Clamping Mandrel|
||0x06|0x14C6|Vise|
||0x07|0x14C7|Zero-Point Clamping System|
||0x08|0x14C8|Torque Wrench|
||0x09|0x14C9|Torque Screwdriver|
|0x054|0x00|0x1500|Generic Cookware Device|
||0x01|0x1501|Pot and Jugs|
|Bluetooth SIG|0x02<br>Proprietary|0x1502|Pressure Cooker<br>Page 38 o|



Page 38 of 442 

**Assigned Numbers** / Document 

**2026-06-23** 

|0x03|0x1503|Slow Cooker|
|---|---|---|
|0x04|0x1504|Steam Cooker|
|0x05|0x1505|Saucepan|
|0x06|0x1506|Frying Pan|
|0x07|0x1507|Casserole|
|0x08|0x1508|Dutch Oven|
|0x09|0x1509|Grill Pan/Raclette Grill/Griddle Pan|
|0x0A|0x150A|Braising Pan|
|0x0B|0x150B|Wok Pan|
|0x0C|0x150C|Paella Pan|
|0x0D|0x150D|Crepe Pan|
|0x0E|0x150E|Tagine|
|0x0F|0x150F|Fondue|
|0x10|0x1510|Lid|
|0x11|0x1511|Wired Probe|
|0x12|0x1512|Wireless Probe|
|0x13|0x1513|Baking Molds|
|0x14|0x1514|Baking Tray|



Bluetooth SIG Proprietary 

Page 39 of 442 

**Assigned Numbers** / Document 

**2026-06-23** 

#### **2.7 URI Scheme Name String Mapping** 

Referenced from Supplement to the Bluetooth Core Specification Part A, Section 1.18.1 [22]. 

Last Modified: 2026-04-30 

Filename: assigned_numbers/core/uri_schemes.yaml 

|**Value**|**URI Scheme**|
|---|---|
|U+0001|empty scheme name|
|U+0002|aaa:|
|U+0003|aaas:|
|U+0004|about:|
|U+0005|acap:|
|U+0006|acct:|
|U+0007|cap:|
|U+0008|cid:|
|U+0009|coap:|
|U+000A|coaps:|
|U+000B|crid:|
|U+000C|data:|
|U+000D|dav:|
|U+000E|dict:|
|U+000F|dns:|
|U+0010|file:|
|U+0011|ftp:|
|U+0012|geo:|
|U+0013|go:|
|U+0014|gopher:|
|U+0015|h323:|
|U+0016|http:|
|U+0017|https:|
|U+0018|iax:|
|U+0019|icap:|
|U+001A|im:|
|U+001B|imap:|
|U+001C|info:|
|U+001D|ipp:|
|U+001E|ipps:|
|U+001F|iris:|



Bluetooth SIG Proprietary 

Page 40 of 442 

**Assigned Numbers** / Document 

**2026-06-23** 

|U+0020|iris.beep:|
|---|---|
|U+0021|iris.xpc:|
|U+0022|iris.xpcs:|
|U+0023|iris.lwz:|
|U+0024|jabber:|
|U+0025|ldap:|
|U+0026|mailto:|
|U+0027|mid:|
|U+0028|msrp:|
|U+0029|msrps:|
|U+002A|mtqp:|
|U+002B|mupdate:|
|U+002C|news:|
|U+002D|nfs:|
|U+002E|ni:|
|U+002F|nih:|
|U+0030|nntp:|
|U+0031|opaquelocktoken:|
|U+0032|pop:|
|U+0033|pres:|
|U+0034|reload:|
|U+0035|rtsp:|
|U+0036|rtsps:|
|U+0037|rtspu:|
|U+0038|service:|
|U+0039|session:|
|U+003A|shttp:|
|U+003B|sieve:|
|U+003C|sip:|
|U+003D|sips:|
|U+003E|sms:|
|U+003F|snmp:|
|U+0040|soap.beep:|
|U+0041|soap.beeps:|
|U+0042|stun:|
|U+0043|stuns:|



Bluetooth SIG Proprietary 

Page 41 of 442 

**Assigned Numbers** / Document 

**2026-06-23** 

|U+0044|tag:|
|---|---|
|U+0045|tel:|
|U+0046|telnet:|
|U+0047|tftp:|
|U+0048|thismessage:|
|U+0049|tn3270:|
|U+004A|tip:|
|U+004B|turn:|
|U+004C|turns:|
|U+004D|tv:|
|U+004E|urn:|
|U+004F|vemmi:|
|U+0050|ws:|
|U+0051|wss:|
|U+0052|xcon:|
|U+0053|xcon-userid:|
|U+0054|xmlrpc.beep:|
|U+0055|xmlrpc.beeps:|
|U+0056|xmpp:|
|U+0057|z39.50r:|
|U+0058|z39.50s:|
|U+0059|acr:|
|U+005A|adiumxtra:|
|U+005B|afp:|
|U+005C|afs:|
|U+005D|aim:|
|U+005E|apt:|
|U+005F|attachment:|
|U+0060|aw:|
|U+0061|barion:|
|U+0062|beshare:|
|U+0063|bitcoin:|
|U+0064|bolo:|
|U+0065|callto:|
|U+0066|chrome:|
|U+0067|chrome-extension:|



Bluetooth SIG Proprietary 

Page 42 of 442 

**Assigned Numbers** / Document 

**2026-06-23** 

|U+0068|com-eventbrite-attendee:|
|---|---|
|U+0069|content:|
|U+006A|cvs:|
|U+006B|dlna-playsingle:|
|U+006C|dlna-playcontainer:|
|U+006D|dtn:|
|U+006E|dvb:|
|U+006F|ed2k:|
|U+0070|facetime:|
|U+0071|feed:|
|U+0072|feedready:|
|U+0073|finger:|
|U+0074|fish:|
|U+0075|gg:|
|U+0076|git:|
|U+0077|gizmoproject:|
|U+0078|gtalk:|
|U+0079|ham:|
|U+007A|hcp:|
|U+007B|icon:|
|U+007C|ipn:|
|U+007D|irc:|
|U+007E|irc6:|
|U+007F|ircs:|
|U+0080|itms:|
|U+0081|jar:|
|U+0082|jms:|
|U+0083|keyparc:|
|U+0084|lastfm:|
|U+0085|ldaps:|
|U+0086|magnet:|
|U+0087|maps:|
|U+0088|market:|
|U+0089|message:|
|U+008A|mms:|
|U+008B|ms-help:|



Bluetooth SIG Proprietary 

Page 43 of 442 

**Assigned Numbers** / Document 

**2026-06-23** 

|U+008C|ms-settings-power:|
|---|---|
|U+008D|msnim:|
|U+008E|mumble:|
|U+008F|mvn:|
|U+0090|notes:|
|U+0091|oid:|
|U+0092|palm:|
|U+0093|paparazzi:|
|U+0094|pkcs11:|
|U+0095|platform:|
|U+0096|proxy:|
|U+0097|psyc:|
|U+0098|query:|
|U+0099|res:|
|U+009A|resource:|
|U+009B|rmi:|
|U+009C|rsync:|
|U+009D|rtmfp:|
|U+009E|rtmp:|
|U+009F|secondlife:|
|U+00A0|sftp:|
|U+00A1|sgn:|
|U+00A2|skype:|
|U+00A3|smb:|
|U+00A4|smtp:|
|U+00A5|soldat:|
|U+00A6|spotify:|
|U+00A7|ssh:|
|U+00A8|steam:|
|U+00A9|submit:|
|U+00AA|svn:|
|U+00AB|teamspeak:|
|U+00AC|teliaeid:|
|U+00AD|things:|
|U+00AE|udp:|
|U+00AF|unreal:|



Bluetooth SIG Proprietary 

Page 44 of 442 

**Assigned Numbers** / Document 

**2026-06-23** 

|U+00B0|ut2004:|
|---|---|
|U+00B1|ventrilo:|
|U+00B2|view-source:|
|U+00B3|webcal:|
|U+00B4|wtai:|
|U+00B5|wyciwyg:|
|U+00B6|xfire:|
|U+00B7|xri:|
|U+00B8|ymsgr:|
|U+00B9|example:|
|U+00BA|ms-settings-cloudstorage:|
|U+00BB|bluetooth:|
|U+00BC|bl:|



Bluetooth SIG Proprietary 

Page 45 of 442 

**Assigned Numbers** / Document 

**2026-06-23** 

#### **2.8 Class of Device** 

Referenced from the following: 

- Bluetooth Core Specification [Vol 2] Part B, Section 6.5.1.4 [4]. 

- Supplement to the Bluetooth Core Specification Part A, Section 1.6.2 [22]. 

The Class of Device is composed of four fields: A Major Service Classes bitfield, a Major Device Class enumerated value, the Minor Device Class value, and a fixed value of 0b00 in the two least significant bits. The format of the Minor Device Class is determined by the Major Device Class value. The structure of the Class of Device is defined below: 



<!-- Start of picture text -->
23 13 12 8 7 2 1 0<br>Major Service Classes Major Device Class Minor Device Class 0b00<br><!-- End of picture text -->

_Figure 2.2: Class of Device format_ 

##### **2.8.1 Major Service Classes** 

###### Last Modified: 2025-09-17 

Filename: assigned_numbers/core/class_of_device.yaml 

|**Bit**|**Class of Device Major Service Class**|
|---|---|
|13|Limited Discoverable Mode|
|14|LE audio|
|15|Reserved for Future Use|
|16|Positioning (Location identification)|
|17|Networking (LAN, Ad hoc, ...)|
|18|Rendering (Printing, Speakers, ...)|
|19|Capturing (Scanner, Microphone, ...)|
|20|Object Transfer (v-Inbox, v-Folder, ...)|
|21|Audio (Speaker, Microphone, Headset service, ...)|
|22|Telephony (Cordless telephony, Modem, Headset service, ...)|
|23|Information (WEB-server, WAP-server, ...)|



Bluetooth SIG Proprietary 

Page 46 of 442 

**Assigned Numbers** / Document 

**2026-06-23** 

##### **2.8.2 Major Device Class** 

The ”Miscellaneous” Major Device Class is used where a more specific Major Device Class code is not suitable. A device that does not have a Major Class Code assigned can use the ”Uncategorized: device code not specified” code until classified. 

Last Modified: 2025-09-17 

Filename: assigned_numbers/core/class_of_device.yaml 

|**12**|**11**|**10**|**9**|**8**|**Major Device Class**|
|---|---|---|---|---|---|
|0|0|0|0|0|Miscellaneous|
|0|0|0|0|1|Computer (desktop, notebook, PDA, organizer, ...)|
|0|0|0|1|0|Phone (cellular, cordless, pay phone, modem, ...)|
|0|0|0|1|1|LAN/Network Access Point|
|0|0|1|0|0|Audio/Video (headset, speaker, stereo, video display, VCR, ...)|
|0|0|1|0|1|Peripheral (mouse, joystick, keyboard, ...)|
|0|0|1|1|0|Imaging (printer, scanner, camera, display, ...)|
|0|0|1|1|1|Wearable|
|0|1|0|0|0|Toy|
|0|1|0|0|1|Health|
|1|1|1|1|1|Uncategorized (device code not specified)|



Bluetooth SIG Proprietary 

Page 47 of 442 

**Assigned Numbers** / Document 

**2026-06-23** 

###### **2.8.2.1 Minor Device Class field – Computer Major Class** 

Last Modified: 2025-09-17 

Filename: assigned_numbers/core/class_of_device.yaml 

|**7**|**6**|**5**|**4**|**3**|**2**|**Minor Device Class**|
|---|---|---|---|---|---|---|
|0|0|0|0|0|0|Uncategorized (code for device not assigned)|
|0|0|0|0|0|1|Desktop Workstation|
|0|0|0|0|1|0|Server-class Computer|
|0|0|0|0|1|1|Laptop|
|0|0|0|1|0|0|Handheld PC/PDA (clamshell)|
|0|0|0|1|0|1|Palm-size PC/PDA|
|0|0|0|1|1|0|Wearable Computer (watch size)|
|0|0|0|1|1|1|Tablet|



Bluetooth SIG Proprietary 

Page 48 of 442 

**Assigned Numbers** / Document 

**2026-06-23** 

###### **2.8.2.2 Minor Device Class field – Phone Major Class** 

Last Modified: 2025-09-17 

Filename: assigned_numbers/core/class_of_device.yaml 

|**7**|**6**|**5**|**4**|**3**|**2**|**Minor Device Class**|
|---|---|---|---|---|---|---|
|0|0|0|0|0|0|Uncategorized (code for device not assigned)|
|0|0|0|0|0|1|Cellular|
|0|0|0|0|1|0|Cordless|
|0|0|0|0|1|1|Smartphone|
|0|0|0|1|0|0|Wired Modem or Voice Gateway|
|0|0|0|1|0|1|Common ISDN Access|



Bluetooth SIG Proprietary 

Page 49 of 442 

**Assigned Numbers** / Document 

**2026-06-23** 

###### **2.8.2.3 Minor Device Class field – LAN/Network Access Point Major Class** 

Last Modified: 2025-09-17 

Filename: assigned_numbers/core/class_of_device.yaml 

|**7**|**6**|**5**|**Minor Device Class**|
|---|---|---|---|
|0|0|0|Fully available|
|0|0|1|1% to 17% utilized|
|0|1|0|17% to 33% utilized|
|0|1|1|33% to 50% utilized|
|1|0|0|50% to 67% utilized|
|1|0|1|67% to 83% utilized|
|1|1|0|83% to 99% utilized|
|1|1|1|No service available|



Last Modified: 2025-09-17 

Filename: assigned_numbers/core/class_of_device.yaml 

**4 3 2 Minor Device Class** 0 0 0 Uncategorized (use this value if no others apply) 

Bluetooth SIG Proprietary 

Page 50 of 442 

**Assigned Numbers** / Document 

**2026-06-23** 

###### **2.8.2.4 Minor Device Class field – Audio/Video Major Class** 

Last Modified: 2025-09-17 

Filename: assigned_numbers/core/class_of_device.yaml 

|**7**|**6**|**5**|**4**|**3**|**2**|**Minor Device Class**|
|---|---|---|---|---|---|---|
|0|0|0|0|0|0|Uncategorized (code not assigned)|
|0|0|0|0|0|1|Wearable Headset Device|
|0|0|0|0|1|0|Hands-free Device|
|0|0|0|0|1|1|Reserved for Future Use|
|0|0|0|1|0|0|Microphone|
|0|0|0|1|0|1|Loudspeaker|
|0|0|0|1|1|0|Headphones|
|0|0|0|1|1|1|Portable Audio|
|0|0|1|0|0|0|Car Audio|
|0|0|1|0|0|1|Set-top box|
|0|0|1|0|1|0|HiFi Audio Device|
|0|0|1|0|1|1|VCR|
|0|0|1|1|0|0|Video Camera|
|0|0|1|1|0|1|Camcorder|
|0|0|1|1|1|0|Video Monitor|
|0|0|1|1|1|1|Video Display and Loudspeaker|
|0|1|0|0|0|0|Video Conferencing|
|0|1|0|0|0|1|Reserved for Future Use|
|0|1|0|0|1|0|Gaming/Toy|
|0|1|0|0|1|1|Hearing Aid|
|0|1|0|1|0|0|Glasses|



Bluetooth SIG Proprietary 

Page 51 of 442 

**Assigned Numbers** / Document 

**2026-06-23** 

###### **2.8.2.5 Minor Device Class field – Peripheral Major Class** 

Last Modified: 2025-09-17 

Filename: assigned_numbers/core/class_of_device.yaml 

|**7**|**6**|**Minor Device Class**|
|---|---|---|
|0|0|Uncategorized (code not assigned)|
|0|1|Keyboard|
|1|0|Pointing Device|
|1|1|Combo Keyboard/Pointing Device|



###### Last Modified: 2025-09-17 

Filename: assigned_numbers/core/class_of_device.yaml 

|**5**|**4**|**3**|**2**|**Minor Device Class**|
|---|---|---|---|---|
|0|0|0|0|Uncategorized (code not assigned)|
|0|0|0|1|Joystick|
|0|0|1|0|Gamepad|
|0|0|1|1|Remote Control|
|0|1|0|0|Sensing Device|
|0|1|0|1|Digitizer Tablet|
|0|1|1|0|Card Reader (e.g., SIM Card Reader)|
|0|1|1|1|Digital Pen|
|1|0|0|0|Handheld Scanner (e.g., barcodes, RFID)|
|1|0|0|1|Handheld Gestural Input Device (e.g., ”wand” form factor)|



Bluetooth SIG Proprietary 

Page 52 of 442 

**Assigned Numbers** / Document 

**2026-06-23** 

###### **2.8.2.6 Minor Device Class field – Imaging Major Class** 

Last Modified: 2025-09-17 

Filename: assigned_numbers/core/class_of_device.yaml 

|**7**|**6**|**5**|**4**|**Minor Device Class**|
|---|---|---|---|---|
|X|X|X|1|Display|
|X|X|1|X|Camera|
|X|1|X|X|Scanner|
|1|X|X|X|Printer|



Last Modified: 2025-09-17 

Filename: assigned_numbers/core/class_of_device.yaml 

|**3**|**2**|**Minor Device Class**|
|---|---|---|
|0|0|Uncategorized (default)|



Bluetooth SIG Proprietary 

Page 53 of 442 

**Assigned Numbers** / Document 

**2026-06-23** 

###### **2.8.2.7 Minor Device Class field – Wearable Major Class** 

Last Modified: 2025-09-17 

Filename: assigned_numbers/core/class_of_device.yaml 

|**7**|**6**|**5**|**4**|**3**|**2**|**Minor Device Class**|
|---|---|---|---|---|---|---|
|0|0|0|0|0|1|Wristwatch|
|0|0|0|0|1|0|Pager|
|0|0|0|0|1|1|Jacket|
|0|0|0|1|0|0|Helmet|
|0|0|0|1|0|1|Glasses|
|0|0|0|1|1|0|Pin (e.g., lapel pin, broach, badge)|



Bluetooth SIG Proprietary 

Page 54 of 442 

**Assigned Numbers** / Document 

**2026-06-23** 

###### **2.8.2.8 Minor Device Class field – Toy Major Class** 

Last Modified: 2025-09-17 

Filename: assigned_numbers/core/class_of_device.yaml 

|**7**|**6**|**5**|**4**|**3**|**2**|**Minor Device Class**|
|---|---|---|---|---|---|---|
|0|0|0|0|0|1|Robot|
|0|0|0|0|1|0|Vehicle|
|0|0|0|0|1|1|Doll/Action Figure|
|0|0|0|1|0|0|Controller|
|0|0|0|1|0|1|Game|



Bluetooth SIG Proprietary 

Page 55 of 442 

**Assigned Numbers** / Document 

**2026-06-23** 

###### **2.8.2.9 Minor Device Class field – Health Major Class** 

Last Modified: 2025-09-17 

Filename: assigned_numbers/core/class_of_device.yaml 

|**7**|**6**|**5**|**4**|**3**|**2**|**Minor Device Class**|
|---|---|---|---|---|---|---|
|0|0|0|0|0|0|Undefined|
|0|0|0|0|0|1|Blood Pressure Monitor|
|0|0|0|0|1|0|Thermometer|
|0|0|0|0|1|1|Weighing Scale|
|0|0|0|1|0|0|Glucose Meter|
|0|0|0|1|0|1|Pulse Oximeter|
|0|0|0|1|1|0|Heart/Pulse Rate Monitor|
|0|0|0|1|1|1|Health Data Display|
|0|0|1|0|0|0|Step Counter|
|0|0|1|0|0|1|Body Composition Analyzer|
|0|0|1|0|1|0|Peak Flow Monitor|
|0|0|1|0|1|1|Medication Monitor|
|0|0|1|1|0|0|Knee Prosthesis|
|0|0|1|1|0|1|Ankle Prosthesis|
|0|0|1|1|1|0|Generic Health Manager|
|0|0|1|1|1|1|Personal Mobility Device|



Bluetooth SIG Proprietary 

Page 56 of 442 

**Assigned Numbers** / Document 

**2026-06-23** 

#### **2.9 LE-U CID** 

Referenced from Bluetooth Core Specification [Vol 3] Part A, Section 2.1 [4]. 

###### Last Modified: 2021-11-24 

|**CID**|**Description**|
|---|---|
|0x0020 to 0x003E|Reserved for future use|



Bluetooth SIG Proprietary 

Page 57 of 442 

**Assigned Numbers** / Document 

**2026-06-23** 

#### **2.10 PCM_Data_Format** 

Reference from Bluetooth Core Specification [Vol 4] Part E, Section 7.1.45 [4]. 

Last Modified: 2024-02-05 

Filename: assigned_numbers/core/pcm_data_format.yaml 

|**Value**|**Name**|**Notes**|
|---|---|---|
|0x00|N/A|This value does not apply to the coding format in<br>use.|
|0x01|1’s complement||
|0x02|2’s complement||
|0x03|Sign-magnitude||
|0x04|Unsigned||



Bluetooth SIG Proprietary 

Page 58 of 442 

**Assigned Numbers** / Document 

**2026-06-23** 

#### **2.11 Coding_Format and Codec_ID (HCI)** 

Referenced from the following: 

- Bluetooth Core Specification [Vol 4] Part E, Section 7.1.45 [4]. 

- Bluetooth Core Specification [Vol 4] Part E, Section 7.1.46 [4]. 

- Bluetooth Core Specification [Vol 4] Part E, Section 7.4.8 [4]. 

- Bluetooth Core Specification [Vol 4] Part E, Section 7.4.10 [4]. 

- Bluetooth Core Specification [Vol 4] Part E, Section 7.8.109 [4]. 

###### Last Modified: 2024-02-05 

Filename: assigned_numbers/core/coding_format.yaml 

|**Value**|**Name**|**Notes**|
|---|---|---|
|0x00|µ-law log||
|0x01|A-law log||
|0x02|CVSD||
|0x03|Transparent|Indicates that the controller does not do any<br>transcoding or resampling. See the command<br>description for restrictions on the use of this value.<br>This is also used for test mode|
|0x04|Linear PCM||
|0x05|mSBC||
|0x06|LC3||
|0x07|G.729A||
|0xFF|Vendor Specific|The codec is vendor-specific, as defined by the<br>following 4 octets in the full coding format.|



Bluetooth SIG Proprietary 

Page 59 of 442 

**Assigned Numbers** / Document 

**2026-06-23** 

#### **2.12 MWS** 

##### **2.12.1 MWS Coexistence Transport Layer** 

Referenced from the following: 

- Bluetooth Core Specification [Vol 4] Part E, Section 7.3.83 [4]. 

- Bluetooth Core Specification [Vol 4] Part E, Section 7.5.11 [4]. 

Last Modified: 2024-02-05 

Filename: assigned_numbers/core/transport_layers.yaml 

|**Value**|**Name**|
|---|---|
|0x00|Disabled|
|0x01|WCI-1 Transport|
|0x02|WCI-2 Transport|



##### **2.12.2 MWS Channel Type** 

Referenced from Bluetooth Core Specification [Vol 4] Part E, Section 7.3.80 [4]. 

Last Modified: 2024-02-05 

Filename: assigned_numbers/core/mws_channel_type.yaml 

|**Value**|**Name**|
|---|---|
|0x00|TDD|
|0x01|FDD|



Bluetooth SIG Proprietary 

Page 60 of 442 

**Assigned Numbers** / Document 

**2026-06-23** 

#### **2.13 AMP** 

The features utilizing the values in this section have been removed from the Core Specification v5.3 and later. 

##### **2.13.1 AMP Controller Type** 

These values are only used in the Bluetooth Core Specification version from v3.0+HS to v5.2. 

Last Modified: 2024-02-05 

Filename: assigned_numbers/core/amp.yaml 

|**Value**|**Controller**|
|---|---|
|0x00|Primary BR/EDR Controller|
|0x01|802.11 AMP Controller|



##### **2.13.2 AMP Security keyIDs** 

These values are only used in the Bluetooth Core Specification version from v3.0+HS to v5.2. 

Last Modified: 2024-02-05 

Filename: assigned_numbers/core/amp.yaml 

|**Key**|**Name**|
|---|---|
|'802b'|802.11 PAL keyID|



##### **2.13.3 AMP Security keyLength** 

These values are only used in the Bluetooth Core Specification version from v3.0+HS to v5.2. 

Last Modified: 2024-02-05 

Filename: assigned_numbers/core/amp.yaml 

|**keyLength**|**Name**|
|---|---|
|32|802.11 PAL keyLength|



##### **2.13.4 PAL Versions** 

These values are only used in the Bluetooth Core Specification version from v3.0+HS to v5.2. 

Last Modified: 2024-02-05 

Filename: assigned_numbers/core/amp.yaml 

|**Core Specification Name**|**PAL Version**|
|---|---|
|Bluetooth Core Specification 3.0 + HS and later|0x01|



Bluetooth SIG Proprietary 

Page 61 of 442 

**Assigned Numbers** / Document 

**2026-06-23** 

## **-** **<u>3 16 bit UUIDs</u>** 

See Bluetooth Core Specification [Vol 3] Part B, Section 2.5.1 [4]. 

#### **3.1 Protocol Identifiers** 

See Bluetooth Core Specification [Vol 3] Part B, Section 5.1.5 [4]. 

Last Modified: 2024-10-18 

Filename: assigned_numbers/uuids/protocol_identifiers.yaml 

|**UUID**|**Protocol**|**Reference**|
|---|---|---|
|0x0001|SDP|Bluetooth Core Specification [4]|
|0x0002|UDP|Personal Area Networking Profile [20]|
|0x0003|RFCOMM|RFCOMM [21]|
|0x0004|TCP|Personal Area Networking Profile [20]|
|0x0005|TCS-BIN|Telephony Control Protocol [23]|
|0x0006|TCS-AT|Telephony Control Protocol [23]|
|0x0007|ATT|Bluetooth Core Specification [4]|
|0x0008|OBEX|IrDA Interoperability [13]|
|0x0009|IP|Personal Area Networking Profile [20]|
|0x000A|FTP|Personal Area Networking Profile [20]|
|0x000C|HTTP|Personal Area Networking Profile [20]|
|0x000E|WSP|WAP Bearer [26]|
|0x000F|BNEP|Bluetooth Network Encapsulation Protocol [5]|
|0x0010|UPNP|Extended Service Discovery Profile for UPnP [8]|
|0x0011|HID Protocol|Human Interface Device Profile [11]|
|0x0012|HardcopyControlChannel|Hardcopy Cable Replacement Profile [9]|
|0x0014|HardcopyDataChannel|Hardcopy Cable Replacement Profile [9]|
|0x0016|HardcopyNotificationChannel|Hardcopy Cable Replacement Profile [9]|
|0x0017|AVCTP|Audio/Video Control Transport Protocol [2]|
|0x0019|AVDTP|Audio/Video Distribution Transport Protocol [3]|
|0x001B|CMTP|Common ISDN Access Profile [6]|
|0x001E|MCAP Control Channel|Health Device Profile [10]|
|0x001F|MCAP Data Channel|Health Device Profile [10]|
|0x0100|L2CAP|Bluetooth Core Specification [4]|



Bluetooth SIG Proprietary 

Page 62 of 442 

**Assigned Numbers** / Document 

**2026-06-23** 

#### **3.2 Browse Group Identifiers** 

Referenced from Bluetooth Core Specification [Vol 3] Part B, Section 5.1.7 [4]. 

Last Modified: 2024-10-18 

Filename: assigned_numbers/uuids/browse_group_identifiers.yaml 

|**UUID**|**Protocol**|
|---|---|
|0x1002|PublicBrowseRoot|



Bluetooth SIG Proprietary 

Page 63 of 442 

**Assigned Numbers** / Document 

**2026-06-23** 

#### **3.3 SDP Service Class and Profile Identifiers** 

See Bluetooth Core Specification [Vol 3] Part B, Section 5.1.2 [4]. 

Last Modified: 2024-10-18 

Filename: assigned_numbers/uuids/service_class.yaml 

|**UUID**|**Name**|**Type**|
|---|---|---|
|0x1000|ServiceDiscoveryServerServiceClassID|Service Class|
|0x1001|BrowseGroupDescriptorServiceClassID|Service Class|
|0x1101|SerialPort|Service Class and Profile|
|0x1102|LANAccessUsingPPP|Service Class and Profile|
|0x1103|Dial-Up Networking|Service Class and Profile|
|0x1104|IrMCSync|Service Class and Profile|
|0x1105|OBEXObjectPush|Service Class and Profile|
|0x1106|OBEX File Transfer|Service Class and Profile|
|0x1107|IrMCSyncCommand|Service Class|
|0x1108|Headset|Service Class and Profile|
|0x1109|CordlessTelephony|Service Class and Profile|
|0x110A|Audio Source|Service Class|
|0x110B|Audio Sink|Service Class|
|0x110C|A/V Remote Control Target|Service Class|
|0x110D|Advanced Audio Distribution|Profile|
|0x110E|A/V Remote Control|Service Class and Profile|
|0x110F|A/V Remote Control Controller|Service Class|
|0x1110|Intercom|Service Class|
|0x1111|Fax|Service Class|
|0x1112|Headset Audio Gateway|Service Class|
|0x1113|WAP|Service Class|
|0x1114|WAP_CLIENT|Service Class|
|0x1115|PANU|Service Class and Profile|
|0x1116|NAP|Service Class and Profile|
|0x1117|GN|Service Class and Profile|
|0x1118|DirectPrinting|Service Class|
|0x1119|ReferencePrinting|Service Class|
|0x111A|Imaging|Profile|
|0x111B|Imaging Responder|Service Class|
|0x111C|Imaging Automatic Archive|Service Class|
|0x111D|Imaging Referenced Objects|Service Class|



Bluetooth SIG Proprietary 

Page 64 of 442 

**Assigned Numbers** / Document 

**2026-06-23** 

|0x111E|Hands-Free|Service Class and Profile|
|---|---|---|
|0x111F|AG Hands-Free|Service Class|
|0x1120|DirectPrintingReferencedObjectsService|Service Class|
|0x1121|ReflectedUI|Service Class|
|0x1122|BasicPrinting|Profile|
|0x1123|PrintingStatus|Service Class|
|0x1124|HID|Service Class and Profile|
|0x1125|HardcopyCableReplacement|Profile|
|0x1126|HCR_Print|Service Class|
|0x1127|HCR_Scan|Service Class|
|0x1128|Common_ISDN_Access|Service Class and Profile|
|0x112D|SIM Access|Service Class and Profile|
|0x112E|Phonebook Access Client|Service Class|
|0x112F|Phonebook Access Server|Service Class|
|0x1130|Phonebook Access Profile|Profile|
|0x1131|Headset - HS|Service Class|
|0x1132|Message Access Server|Service Class|
|0x1133|Message Notification Server|Service Class|
|0x1134|Message Access Profile|Profile|
|0x1135|GNSS|Profile|
|0x1136|GNSS_Server|Service Class|
|0x1137|3D Display|Service Class|
|0x1138|3D Glasses|Service Class|
|0x1139|3D Synch Profile|Profile|
|0x113A|Multi Profile Specification|Profile|
|0x113B|MPS|Service Class|
|0x113C|CTN Access Service|Service Class|
|0x113D|CTN Notification Service|Service Class|
|0x113E|Calendar Tasks and Notes Profile|Profile|
|0x1200|PnPInformation|Service Class and Profile|
|0x1201|Generic Networking|Service Class|
|0x1202|GenericFileTransfer|Service Class|
|0x1203|Generic Audio|Service Class|
|0x1204|GenericTelephony|Service Class|
|0x1205|UPNP_Service|Service Class|
|0x1206<br>Blueto|UPNP_IP_Service<br>oth SIG Proprietary|Service Class|



Page 65 of 442 

**Assigned Numbers** / Document 

**2026-06-23** 

|0x1300|ESDP_UPNP_IP_PAN|Service Class|
|---|---|---|
|0x1301|ESDP_UPNP_IP_LAP|Service Class|
|0x1302|ESDP_UPNP_L2CAP|Service Class|
|0x1303|Video Source|Service Class|
|0x1304|Video Sink|Service Class|
|0x1305|Video Distribution|Profile|
|0x1400|HDP|Profile|
|0x1401|HDP Source|Service Class|
|0x1402|HDP Sink|Service Class|



Bluetooth SIG Proprietary 

Page 66 of 442 

**Assigned Numbers** / Document 

**2026-06-23** 

#### **3.4 GATT Services** 

See Bluetooth Core Specification [Vol 3] Part G, Section 3.3.1 [4]. 

##### **3.4.1 Services by Name** 

###### Last Modified: 2025-12-16 

Filename: assigned_numbers/uuids/service_uuids.yaml 

|**Name**|**UUID**|
|---|---|
|Alert Notification Service|0x1811|
|Audio Input Control Service|0x1843|
|Audio Stream Control Service|0x184E|
|Authorization Control Service|0x183D|
|Automation IO Service|0x1815|
|Basic Audio Announcement Service|0x1851|
|Battery Service|0x180F|
|Binary Sensor Service|0x183B|
|Blood Pressure Service|0x1810|
|Body Composition Service|0x181B|
|Bond Management Service|0x181E|
|Broadcast Audio Announcement Service|0x1852|
|Broadcast Audio Scan Service|0x184F|
|Common Audio Service|0x1853|
|Constant Tone Extension Service|0x184A|
|Continuous Glucose Monitoring Service|0x181F|
|Cookware Service|0x185D|
|Coordinated Set Identification Service|0x1846|
|Current Time Service|0x1805|
|Cycling Power Service|0x1818|
|Cycling Speed and Cadence Service|0x1816|
|Device Information Service|0x180A|
|Device Time Service|0x1847|
|Elapsed Time Service|0x183F|
|Electronic Shelf Label Service|0x1857|
|Emergency Configuration Service|0x183C|
|Environmental Sensing Service|0x181A|
|Fitness Machine Service|0x1826|



Bluetooth SIG Proprietary 

Page 67 of 442 

**Assigned Numbers** / Document 

**2026-06-23** 

|Gaming Audio Service|0x1858|
|---|---|
|GAP Service|0x1800|
|GATT Service|0x1801|
|Generic Health Sensor Service|0x1840|
|Generic Media Control Service|0x1849|
|Generic Telephone Bearer Service|0x184C|
|Generic Voice Assistant Service|0x185F|
|Glucose Service|0x1808|
|Health Thermometer Service|0x1809|
|Hearing Access Service|0x1854|
|Heart Rate Service|0x180D|
|HID ISO Service|0x185C|
|HTTP Proxy Service|0x1823|
|Human Interface Device Service|0x1812|
|Immediate Alert Service|0x1802|
|Indoor Positioning Service|0x1821|
|Industrial Measurement Device Service|0x185A|
|Insulin Delivery Service|0x183A|
|Internet Protocol Support Service|0x1820|
|Link Loss Service|0x1803|
|Location and Navigation Service|0x1819|
|Media Control Service|0x1848|
|Mesh Provisioning Service|0x1827|
|Mesh Proxy Service|0x1828|
|Mesh Proxy Solicitation Service|0x1859|
|Microphone Control Service|0x184D|
|Next DST Change Service|0x1807|
|Object Transfer Service|0x1825|
|Phone Alert Status Service|0x180E|
|Physical Activity Monitor Service|0x183E|
|Public Broadcast Announcement Service|0x1856|
|Published Audio Capabilities Service|0x1850|
|Pulse Oximeter Service|0x1822|
|Ranging Service|0x185B|
|Reconnection Configuration Service|0x1829|
|Reference Time Update Service<br>Bluetooth SIG Proprietary|0x1806|



Page 68 of 442 

**Assigned Numbers** / Document 

**2026-06-23** 

|Running Speed and Cadence Service|0x1814|
|---|---|
|Scan Parameters Service|0x1813|
|Telephone Bearer Service|0x184B|
|Telephony and Media Audio Service|0x1855|
|Transport Discovery Service|0x1824|
|Tx Power Service|0x1804|
|User Data Service|0x181C|
|Voice Assistant Service|0x185E|
|Volume Control Service|0x1844|
|Volume Offset Control Service|0x1845|
|Weight Scale Service|0x181D|



Bluetooth SIG Proprietary 

Page 69 of 442 

**Assigned Numbers** / Document 

**2026-06-23** 

##### **3.4.2 Services by UUID** 

###### Last Modified: 2025-12-16 

Filename: assigned_numbers/uuids/service_uuids.yaml 

|**UUID**|**Name**|
|---|---|
|0x1800|GAP Service|
|0x1801|GATT Service|
|0x1802|Immediate Alert Service|
|0x1803|Link Loss Service|
|0x1804|Tx Power Service|
|0x1805|Current Time Service|
|0x1806|Reference Time Update Service|
|0x1807|Next DST Change Service|
|0x1808|Glucose Service|
|0x1809|Health Thermometer Service|
|0x180A|Device Information Service|
|0x180D|Heart Rate Service|
|0x180E|Phone Alert Status Service|
|0x180F|Battery Service|
|0x1810|Blood Pressure Service|
|0x1811|Alert Notification Service|
|0x1812|Human Interface Device Service|
|0x1813|Scan Parameters Service|
|0x1814|Running Speed and Cadence Service|
|0x1815|Automation IO Service|
|0x1816|Cycling Speed and Cadence Service|
|0x1818|Cycling Power Service|
|0x1819|Location and Navigation Service|
|0x181A|Environmental Sensing Service|
|0x181B|Body Composition Service|
|0x181C|User Data Service|
|0x181D|Weight Scale Service|
|0x181E|Bond Management Service|
|0x181F|Continuous Glucose Monitoring Service|
|0x1820|Internet Protocol Support Service|
|0x1821|Indoor Positioning Service|



Bluetooth SIG Proprietary 

Page 70 of 442 

**Assigned Numbers** / Document 

**2026-06-23** 

|0x1822|Pulse Oximeter Service|
|---|---|
|0x1823|HTTP Proxy Service|
|0x1824|Transport Discovery Service|
|0x1825|Object Transfer Service|
|0x1826|Fitness Machine Service|
|0x1827|Mesh Provisioning Service|
|0x1828|Mesh Proxy Service|
|0x1829|Reconnection Configuration Service|
|0x183A|Insulin Delivery Service|
|0x183B|Binary Sensor Service|
|0x183C|Emergency Configuration Service|
|0x183D|Authorization Control Service|
|0x183E|Physical Activity Monitor Service|
|0x183F|Elapsed Time Service|
|0x1840|Generic Health Sensor Service|
|0x1843|Audio Input Control Service|
|0x1844|Volume Control Service|
|0x1845|Volume Offset Control Service|
|0x1846|Coordinated Set Identification Service|
|0x1847|Device Time Service|
|0x1848|Media Control Service|
|0x1849|Generic Media Control Service|
|0x184A|Constant Tone Extension Service|
|0x184B|Telephone Bearer Service|
|0x184C|Generic Telephone Bearer Service|
|0x184D|Microphone Control Service|
|0x184E|Audio Stream Control Service|
|0x184F|Broadcast Audio Scan Service|
|0x1850|Published Audio Capabilities Service|
|0x1851|Basic Audio Announcement Service|
|0x1852|Broadcast Audio Announcement Service|
|0x1853|Common Audio Service|
|0x1854|Hearing Access Service|
|0x1855|Telephony and Media Audio Service|
|0x1856|Public Broadcast Announcement Service|
|0x1857|Electronic Shelf Label Service|



Bluetooth SIG Proprietary 

Page 71 of 442 

**Assigned Numbers** / Document 

**2026-06-23** 

|0x1858|Gaming Audio Service|
|---|---|
|0x1859|Mesh Proxy Solicitation Service|
|0x185A|Industrial Measurement Device Service|
|0x185B|Ranging Service|
|0x185C|HID ISO Service|
|0x185D|Cookware Service|
|0x185E|Voice Assistant Service|
|0x185F|Generic Voice Assistant Service|



Bluetooth SIG Proprietary 

Page 72 of 442 

**Assigned Numbers** / Document 

**2026-06-23** 

#### **3.5 Units** 

Referenced from Bluetooth Core Specification [Vol 3] Part G, Section 3.3.3.5.4 [4]. 

##### **3.5.1 Units by Name** 

###### Last Modified: 2025-07-10 

Filename: assigned_numbers/uuids/units.yaml 

|**Unit Name**|**UUID**|
|---|---|
|absorbed dose (gray)|0x2733|
|absorbed dose rate (gray per second)|0x2754|
|acceleration (metres per second squared)|0x2713|
|activity referred to a radionuclide (becquerel)|0x2732|
|amount concentration (mole per cubic metre)|0x271A|
|amount of substance (mole)|0x2706|
|angular acceleration (radian per second squared)|0x2744|
|angular velocity (radian per second)|0x2743|
|angular velocity (revolution per minute)|0x27A8|
|area (barn)|0x2784|
|area (hectare)|0x2766|
|area (square metres)|0x2710|
|capacitance (farad)|0x2729|
|catalytic activity (katal)|0x2735|
|catalytic activity concentration (katal per cubic metre)|0x2757|
|Celsius temperature (degree Celsius)|0x272F|
|concentration (count per cubic metre)|0x27B5|
|current density (ampere per square metre)|0x2718|
|density (kilogram per cubic metre)|0x2715|
|dose equivalent (sievert)|0x2734|
|dynamic viscosity (pascal second)|0x2740|
|electric charge (ampere hours)|0x27B0|
|electric charge (coulomb)|0x2727|
|electric charge density (coulomb per cubic metre)|0x274C|
|electric conductance (siemens)|0x272B|
|electric current (ampere)|0x2704|
|electric field strength (volt per metre)|0x274B|
|electric flux density (coulomb per square metre)|0x274E|



Bluetooth SIG Proprietary 

Page 73 of 442 

**Assigned Numbers** / Document 

**2026-06-23** 

|electric potential difference (volt)|0x2728|
|---|---|
|electric resistance (ohm)|0x272A|
|Electrical Apparent Energy (kilovolt ampere hour)|0x27C7|
|Electrical Apparent Power (volt ampere)|0x27C8|
|energy (gram calorie)|0x27A9|
|energy (joule)|0x2725|
|energy (kilogram calorie)|0x27AA|
|energy (kilowatt hour)|0x27AB|
|energy density (joule per cubic metre)|0x274A|
|exposure (coulomb per kilogram)|0x2753|
|force (newton)|0x2723|
|frequency (hertz)|0x2722|
|Gravity (gn)|0x27C9|
|heat capacity (joule per kelvin)|0x2746|
|heat flux density (watt per square metre)|0x2745|
|illuminance (lux)|0x2731|
|inductance (henry)|0x272E|
|irradiance (watt per square metre)|0x27B6|
|length (foot)|0x27A3|
|length (inch)|0x27A2|
|length (metre)|0x2701|
|length (mile)|0x27A4|
|length (nautical mile)|0x2783|
|length (parsec)|0x27A1|
|length (yard)|0x27A0|
|length (ångström)|0x2782|
|logarithmic radio quantity (bel)|0x2787|
|logarithmic radio quantity (neper)|0x2786|
|luminance (candela per square metre)|0x271C|
|luminous efficacy (lumen per watt)|0x27BE|
|luminous energy (lumen hour)|0x27BF|
|luminous exposure (lux hour)|0x27C0|
|luminous flux (lumen)|0x2730|
|luminous intensity (candela)|0x2707|
|magnetic field strength (ampere per metre)|0x2719|
|magnetic flux (weber)<br>Bluetooth SIG Proprietary|0x272C|



Page 74 of 442 

**Assigned Numbers** / Document 

**2026-06-23** 

|magnetic flux density (tesla)|0x272D|
|---|---|
|mass (kilogram)|0x2702|
|mass (pound)|0x27B8|
|mass (tonne)|0x2768|
|mass concentration (kilogram per cubic metre)|0x271B|
|mass density (milligram per decilitre)|0x27B1|
|mass density (millimole per litre)|0x27B2|
|mass density rate ((milligram per decilitre) per minute)|0x27C6|
|mass flow (gram per second)|0x27C1|
|metabolic equivalent|0x27B9|
|milliliter (per kilogram per minute)|0x27B7|
|molar energy (joule per mole)|0x2751|
|molar entropy (joule per mole kelvin)|0x2752|
|moment of force (newton metre)|0x2741|
|pace (kilometre per minute)|0x27BD|
|parts per billion|0x27C5|
|parts per million|0x27C4|
|per mille|0x27AE|
|percentage|0x27AD|
|period (beats per minute)|0x27AF|
|permeability (henry per metre)|0x2750|
|permittivity (farad per metre)|0x274F|
|plane angle (degree)|0x2763|
|plane angle (minute)|0x2764|
|plane angle (radian)|0x2720|
|plane angle (second)|0x2765|
|power (watt)|0x2726|
|pressure (bar)|0x2780|
|pressure (millimetre of mercury)|0x2781|
|pressure (pascal)|0x2724|
|pressure (pound-force per square inch)|0x27A5|
|radiance (watt per square metre steradian)|0x2756|
|radiant intensity (watt per steradian)|0x2755|
|refractive index|0x271D|
|relative permeability|0x271E|
|solid angle (steradian)<br>Bluetooth SIG Proprietary|0x2721|



Page 75 of 442 

**Assigned Numbers** / Document 

**2026-06-23** 

|sound pressure (decibel)|0x27C3|
|---|---|
|specific energy (joule per kilogram)|0x2748|
|specific heat capacity (joule per kilogram kelvin)|0x2747|
|specific volume (cubic metre per kilogram)|0x2717|
|step (per minute)|0x27BA|
|stroke (per minute)|0x27BC|
|surface charge density (coulomb per square metre)|0x274D|
|surface density (kilogram per square metre)|0x2716|
|surface tension (newton per metre)|0x2742|
|thermal conductivity (watt per metre kelvin)|0x2749|
|thermodynamic temperature (degree Fahrenheit)|0x27AC|
|thermodynamic temperature (kelvin)|0x2705|
|time (day)|0x2762|
|time (hour)|0x2761|
|time (minute)|0x2760|
|time (month)|0x27B4|
|time (second)|0x2703|
|time (year)|0x27B3|
|unitless|0x2700|
|velocity (kilometre per hour)|0x27A6|
|velocity (knot)|0x2785|
|velocity (metres per second)|0x2712|
|velocity (mile per hour)|0x27A7|
|volume (cubic metres)|0x2711|
|volume (litre)|0x2767|
|volume flow (litre per second)|0x27C2|
|wavenumber (reciprocal metre)|0x2714|



Bluetooth SIG Proprietary 

Page 76 of 442 

**Assigned Numbers** / Document 

**2026-06-23** 

##### **3.5.2 Units by UUID** 

###### Last Modified: 2025-07-10 

Filename: assigned_numbers/uuids/units.yaml 

|**UUID**|**Unit Name**|
|---|---|
|0x2700|unitless|
|0x2701|length (metre)|
|0x2702|mass (kilogram)|
|0x2703|time (second)|
|0x2704|electric current (ampere)|
|0x2705|thermodynamic temperature (kelvin)|
|0x2706|amount of substance (mole)|
|0x2707|luminous intensity (candela)|
|0x2710|area (square metres)|
|0x2711|volume (cubic metres)|
|0x2712|velocity (metres per second)|
|0x2713|acceleration (metres per second squared)|
|0x2714|wavenumber (reciprocal metre)|
|0x2715|density (kilogram per cubic metre)|
|0x2716|surface density (kilogram per square metre)|
|0x2717|specific volume (cubic metre per kilogram)|
|0x2718|current density (ampere per square metre)|
|0x2719|magnetic field strength (ampere per metre)|
|0x271A|amount concentration (mole per cubic metre)|
|0x271B|mass concentration (kilogram per cubic metre)|
|0x271C|luminance (candela per square metre)|
|0x271D|refractive index|
|0x271E|relative permeability|
|0x2720|plane angle (radian)|
|0x2721|solid angle (steradian)|
|0x2722|frequency (hertz)|
|0x2723|force (newton)|
|0x2724|pressure (pascal)|
|0x2725|energy (joule)|
|0x2726|power (watt)|
|0x2727|electric charge (coulomb)|



Bluetooth SIG Proprietary 

Page 77 of 442 

**Assigned Numbers** / Document 

**2026-06-23** 

|0x2728|electric potential difference (volt)|
|---|---|
|0x2729|capacitance (farad)|
|0x272A|electric resistance (ohm)|
|0x272B|electric conductance (siemens)|
|0x272C|magnetic flux (weber)|
|0x272D|magnetic flux density (tesla)|
|0x272E|inductance (henry)|
|0x272F|Celsius temperature (degree Celsius)|
|0x2730|luminous flux (lumen)|
|0x2731|illuminance (lux)|
|0x2732|activity referred to a radionuclide (becquerel)|
|0x2733|absorbed dose (gray)|
|0x2734|dose equivalent (sievert)|
|0x2735|catalytic activity (katal)|
|0x2740|dynamic viscosity (pascal second)|
|0x2741|moment of force (newton metre)|
|0x2742|surface tension (newton per metre)|
|0x2743|angular velocity (radian per second)|
|0x2744|angular acceleration (radian per second squared)|
|0x2745|heat flux density (watt per square metre)|
|0x2746|heat capacity (joule per kelvin)|
|0x2747|specific heat capacity (joule per kilogram kelvin)|
|0x2748|specific energy (joule per kilogram)|
|0x2749|thermal conductivity (watt per metre kelvin)|
|0x274A|energy density (joule per cubic metre)|
|0x274B|electric field strength (volt per metre)|
|0x274C|electric charge density (coulomb per cubic metre)|
|0x274D|surface charge density (coulomb per square metre)|
|0x274E|electric flux density (coulomb per square metre)|
|0x274F|permittivity (farad per metre)|
|0x2750|permeability (henry per metre)|
|0x2751|molar energy (joule per mole)|
|0x2752|molar entropy (joule per mole kelvin)|
|0x2753|exposure (coulomb per kilogram)|
|0x2754|absorbed dose rate (gray per second)|
|0x2755|radiant intensity (watt per steradian)|



Bluetooth SIG Proprietary 

Page 78 of 442 

**Assigned Numbers** / Document 

**2026-06-23** 

|0x2756|radiance (watt per square metre steradian)|
|---|---|
|0x2757|catalytic activity concentration (katal per cubic metre)|
|0x2760|time (minute)|
|0x2761|time (hour)|
|0x2762|time (day)|
|0x2763|plane angle (degree)|
|0x2764|plane angle (minute)|
|0x2765|plane angle (second)|
|0x2766|area (hectare)|
|0x2767|volume (litre)|
|0x2768|mass (tonne)|
|0x2780|pressure (bar)|
|0x2781|pressure (millimetre of mercury)|
|0x2782|length (ångström)|
|0x2783|length (nautical mile)|
|0x2784|area (barn)|
|0x2785|velocity (knot)|
|0x2786|logarithmic radio quantity (neper)|
|0x2787|logarithmic radio quantity (bel)|
|0x27A0|length (yard)|
|0x27A1|length (parsec)|
|0x27A2|length (inch)|
|0x27A3|length (foot)|
|0x27A4|length (mile)|
|0x27A5|pressure (pound-force per square inch)|
|0x27A6|velocity (kilometre per hour)|
|0x27A7|velocity (mile per hour)|
|0x27A8|angular velocity (revolution per minute)|
|0x27A9|energy (gram calorie)|
|0x27AA|energy (kilogram calorie)|
|0x27AB|energy (kilowatt hour)|
|0x27AC|thermodynamic temperature (degree Fahrenheit)|
|0x27AD|percentage|
|0x27AE|per mille|
|0x27AF|period (beats per minute)|
|0x27B0|electric charge (ampere hours)|



Bluetooth SIG Proprietary 

Page 79 of 442 

**Assigned Numbers** / Document 

**2026-06-23** 

|0x27B1|mass density (milligram per decilitre)|
|---|---|
|0x27B2|mass density (millimole per litre)|
|0x27B3|time (year)|
|0x27B4|time (month)|
|0x27B5|concentration (count per cubic metre)|
|0x27B6|irradiance (watt per square metre)|
|0x27B7|milliliter (per kilogram per minute)|
|0x27B8|mass (pound)|
|0x27B9|metabolic equivalent|
|0x27BA|step (per minute)|
|0x27BC|stroke (per minute)|
|0x27BD|pace (kilometre per minute)|
|0x27BE|luminous efficacy (lumen per watt)|
|0x27BF|luminous energy (lumen hour)|
|0x27C0|luminous exposure (lux hour)|
|0x27C1|mass flow (gram per second)|
|0x27C2|volume flow (litre per second)|
|0x27C3|sound pressure (decibel)|
|0x27C4|parts per million|
|0x27C5|parts per billion|
|0x27C6|mass density rate ((milligram per decilitre) per minute)|
|0x27C7|Electrical Apparent Energy (kilovolt ampere hour)|
|0x27C8|Electrical Apparent Power (volt ampere)|
|0x27C9|Gravity (gn)|



Bluetooth SIG Proprietary 

Page 80 of 442 

**Assigned Numbers** / Document 

**2026-06-23** 

#### **3.6 Declarations** 

See Bluetooth Core Specification [Vol 3] Part G, Section 3.1 [4]. 

Last Modified: 2024-10-18 

Filename: assigned_numbers/uuids/declarations.yaml 

|**UUID**|**Declaration Name**|
|---|---|
|0x2800|Primary Service|
|0x2801|Secondary Service|
|0x2802|Include|
|0x2803|Characteristic|



Bluetooth SIG Proprietary 

Page 81 of 442 

**Assigned Numbers** / Document 

**2026-06-23** 

#### **3.7 Descriptors** 

Referenced from Bluetooth Core Specification [Vol 3] Part F, Section 2 [4]. 

See Bluetooth Core Specification [Vol 3] Part G, Section 3.3.3 [4]. 

Last Modified: 2025-10-31 

Filename: assigned_numbers/uuids/descriptors.yaml 

|**UUID**|**Descriptor Name**|
|---|---|
|0x2900|Characteristic Extended Properties|
|0x2901|Characteristic User Description|
|0x2902|Client Characteristic Configuration|
|0x2903|Server Characteristic Configuration|
|0x2904|Characteristic Presentation Format|
|0x2905|Characteristic Aggregate Format|
|0x2906|Valid Range|
|0x2907|External Report Reference|
|0x2908|Report Reference|
|0x2909|Number of Digitals|
|0x290A|Value Trigger Setting|
|0x290B|Environmental Sensing Configuration|
|0x290C|Environmental Sensing Measurement|
|0x290D|Environmental Sensing Trigger Setting|
|0x290E|Time Trigger Setting|
|0x290F|Complete BR-EDR Transport Block Data|
|0x2910|Observation Schedule|
|0x2911|Valid Range and Accuracy|
|0x2912|Measurement Description|
|0x2913|Manufacturer Limits|
|0x2914|Process Tolerances|
|0x2915|IMD Trigger Setting|
|0x2916|Cooking Sensor Info|
|0x2917|Cooking Trigger Setting|



Bluetooth SIG Proprietary 

Page 82 of 442 

**Assigned Numbers** / Document 

**2026-06-23** 

#### **3.8 Characteristics** 

Referenced from the following: 

- Bluetooth Core Specification [Vol 3] Part C, Section 12 [4]. 

- Bluetooth Core Specification [Vol 3] Part F, Section 2 [4]. 

See Bluetooth Core Specification [Vol 3] Part G, Section 3.3 [4]. 

##### **3.8.1 Characteristics by Name** 

###### Last Modified: 2026-04-23 

Filename: assigned_numbers/uuids/characteristic_uuids.yaml 

|**Characteristic Name**|**UUID**|
|---|---|
|Acceleration|0x2C06|
|Acceleration - 3D|0x2C1D|
|Acceleration Detection Status|0x2C1F|
|ACS Control Point|0x2B33|
|ACS Data In|0x2B30|
|ACS Data Out Indicate|0x2B32|
|ACS Data Out Notify|0x2B31|
|ACS Status|0x2B2F|
|Active Preset Index|0x2BDC|
|Activity Goal|0x2B4E|
|Advertising Constant Tone Extension Interval|0x2BB1|
|Advertising Constant Tone Extension Minimum Length|0x2BAE|
|Advertising Constant Tone Extension Minimum Transmit Count|0x2BAF|
|Advertising Constant Tone Extension PHY|0x2BB2|
|Advertising Constant Tone Extension Transmit Duration|0x2BB0|
|Aerobic Heart Rate Lower Limit|0x2A7E|
|Aerobic Heart Rate Upper Limit|0x2A84|
|Aerobic Threshold|0x2A7F|
|Age|0x2A80|
|Aggregate|0x2A5A|
|Alert Category ID|0x2A43|
|Alert Category ID Bit Mask|0x2A42|
|Alert Level|0x2A06|
|Alert Notification Control Point|0x2A44|
|Alert Status|0x2A3F|



Bluetooth SIG Proprietary 

Page 83 of 442 

**Assigned Numbers** / Document 

**2026-06-23** 

|Altitude|0x2AB3|
|---|---|
|Ammonia Concentration|0x2BCF|
|Anaerobic Heart Rate Lower Limit|0x2A81|
|Anaerobic Heart Rate Upper Limit|0x2A82|
|Anaerobic Threshold|0x2A83|
|AP Sync Key Material|0x2BF7|
|Apparent Energy 32|0x2B89|
|Apparent Power|0x2B8A|
|Apparent Wind Direction|0x2A73|
|Apparent Wind Speed|0x2A72|
|Appearance|0x2A01|
|ASE Control Point|0x2BC6|
|Audio Input Control Point|0x2B7B|
|Audio Input Description|0x2B7C|
|Audio Input State|0x2B77|
|Audio Input Status|0x2B7A|
|Audio Input Type|0x2B79|
|Audio Location|0x2B81|
|Audio Output Description|0x2B83|
|Available Audio Contexts|0x2BCD|
|Average Current|0x2AE0|
|Average Voltage|0x2AE1|
|Barometric Pressure Trend|0x2AA3|
|Battery Critical Status|0x2BE9|
|Battery Energy Status|0x2BF0|
|Battery Health Information|0x2BEB|
|Battery Health Status|0x2BEA|
|Battery Information|0x2BEC|
|Battery Level|0x2A19|
|Battery Level Status|0x2BED|
|Battery Time Status|0x2BEE|
|Bearer List Current Calls|0x2BB9|
|Bearer Provider Name|0x2BB3|
|Bearer Signal Strength|0x2BB7|
|Bearer Signal Strength Reporting Interval|0x2BB8|
|Bearer Technology<br>Bluetooth SIG Proprietary|0x2BB5|



Page 84 of 442 

**Assigned Numbers** / Document 

**2026-06-23** 

|Bearer UCI|0x2BB4|
|---|---|
|Bearer URI Schemes Supported List|0x2BB6|
|BGR Features|0x2C04|
|BGS Features|0x2C03|
|Blood Pressure Feature|0x2A49|
|Blood Pressure Measurement|0x2A35|
|Blood Pressure Record|0x2B36|
|Bluetooth SIG Data|0x2B39|
|Body Composition Feature|0x2A9B|
|Body Composition Measurement|0x2A9C|
|Body Sensor Location|0x2A38|
|Bond Management Control Point|0x2AA4|
|Bond Management Feature|0x2AA5|
|Boolean|0x2AE2|
|Boot Keyboard Input Report|0x2A22|
|Boot Keyboard Output Report|0x2A32|
|Boot Mouse Input Report|0x2A33|
|BR-EDR Handover Data|0x2B38|
|Broadcast Audio Scan Control Point|0x2BC7|
|Broadcast Receive State|0x2BC8|
|BSS Control Point|0x2B2B|
|BSS Response|0x2B2C|
|Call Control Point|0x2BBE|
|Call Control Point Optional Opcodes|0x2BBF|
|Call Friendly Name|0x2BC2|
|Call State|0x2BBD|
|Caloric Intake|0x2B50|
|Carbon Monoxide Concentration|0x2BD0|
|CardioRespiratory Activity Instantaneous Data|0x2B3E|
|CardioRespiratory Activity Summary Data|0x2B3F|
|Central Address Resolution|0x2AA6|
|CGM Feature|0x2AA8|
|CGM Measurement|0x2AA7|
|CGM Session Run Time|0x2AAB|
|CGM Session Start Time|0x2AAA|
|CGM Specific Ops Control Point<br>Bluetooth SIG Proprietary|0x2AAC|



Page 85 of 442 

**Assigned Numbers** / Document 

**2026-06-23** 

|CGM Status|0x2AA9|
|---|---|
|Chromatic Distance from Planckian|0x2AE3|
|Chromaticity Coordinate|0x2B1C|
|Chromaticity Coordinates|0x2AE4|
|Chromaticity in CCT and Duv Values|0x2AE5|
|Chromaticity Tolerance|0x2AE6|
|CIE 13.3-1995 Color Rendering Index|0x2AE7|
|Client Supported Features|0x2B29|
|CO2 Concentration|0x2B8C|
|Coefficient|0x2AE8|
|Constant Tone Extension Enable|0x2BAD|
|Contact Status 8|0x2C22|
|Content Control ID|0x2BBA|
|Cooking Step Status|0x2C28|
|Cooking Temperature|0x2C2E|
|Cooking Zone Actual Cooking Conditions|0x2C2B|
|Cooking Zone Capabilities|0x2C29|
|Cooking Zone Desired Cooking Conditions|0x2C2A|
|Cooking Zone Perceived Power|0x2C2F|
|Cookware Description|0x2C25|
|Cookware Sensor Aggregate|0x2C2D|
|Cookware Sensor Data|0x2C2C|
|Coordinated Set Name|0x2C1A|
|Coordinated Set Size|0x2B85|
|Correlated Color Temperature|0x2AE9|
|Cosine of the Angle|0x2B8D|
|Count 16|0x2AEA|
|Count 24|0x2AEB|
|Country Code|0x2AEC|
|Cross Trainer Data|0x2ACE|
|CSC Feature|0x2A5C|
|CSC Measurement|0x2A5B|
|Current Group Object ID|0x2BA0|
|Current Time|0x2A2B|
|Current Track Object ID|0x2B9D|
|Current Track Segments Object ID<br>Bluetooth SIG Proprietary|0x2B9C|



Page 86 of 442 

**Assigned Numbers** / Document 

**2026-06-23** 

|Cycling Power Control Point|0x2A66|
|---|---|
|Cycling Power Feature|0x2A65|
|Cycling Power Measurement|0x2A63|
|Cycling Power Vector|0x2A64|
|Database Change Increment|0x2A99|
|Database Hash|0x2B2A|
|Date of Birth|0x2A85|
|Date of Threshold Assessment|0x2A86|
|Date Time|0x2A08|
|Date UTC|0x2AED|
|Day Date Time|0x2A0A|
|Day of Week|0x2A09|
|Descriptor Value Changed|0x2A7D|
|Device Name|0x2A00|
|Device Time|0x2B90|
|Device Time Control Point|0x2B91|
|Device Time Feature|0x2B8E|
|Device Time Parameters|0x2B8F|
|Device Wearing Position|0x2B4B|
|Dew Point|0x2A7B|
|Door/Window Status|0x2C20|
|DST Offset|0x2A0D|
|Elapsed Time|0x2BF2|
|Electric Current|0x2AEE|
|Electric Current Range|0x2AEF|
|Electric Current Specification|0x2AF0|
|Electric Current Statistics|0x2AF1|
|Elevation|0x2A6C|
|Email Address|0x2A87|
|Emergency ID|0x2B2D|
|Emergency Text|0x2B2E|
|Encrypted Data Key Material|0x2B88|
|Energy|0x2AF2|
|Energy 32|0x2BA8|
|Energy in a Period of Day|0x2AF3|
|Enhanced Blood Pressure Measurement|0x2B34|



Bluetooth SIG Proprietary 

Page 87 of 442 

**Assigned Numbers** / Document 

**2026-06-23** 

|Enhanced Intermediate Cuff Pressure|0x2B35|
|---|---|
|ESL Address|0x2BF6|
|ESL Control Point|0x2BFE|
|ESL Current Absolute Time|0x2BF9|
|ESL Display Information|0x2BFA|
|ESL Image Information|0x2BFB|
|ESL LED Information|0x2BFD|
|ESL Response Key Material|0x2BF8|
|ESL Sensor Information|0x2BFC|
|Estimated Service Date|0x2BEF|
|Event Statistics|0x2AF4|
|Exact Time 256|0x2A0C|
|Fat Burn Heart Rate Lower Limit|0x2A88|
|Fat Burn Heart Rate Upper Limit|0x2A89|
|Firmware Revision String|0x2A26|
|First Name|0x2A8A|
|First Use Date|0x2C0E|
|Fitness Machine Control Point|0x2AD9|
|Fitness Machine Feature|0x2ACC|
|Fitness Machine Status|0x2ADA|
|Five Zone Heart Rate Limits|0x2A8B|
|Fixed String 16|0x2AF5|
|Fixed String 24|0x2AF6|
|Fixed String 36|0x2AF7|
|Fixed String 64|0x2BDE|
|Fixed String 8|0x2AF8|
|Floor Number|0x2AB2|
|Force|0x2C07|
|Four Zone Heart Rate Limits|0x2B4C|
|Gain Settings Attribute|0x2B78|
|Gender|0x2A8C|
|General Activity Instantaneous Data|0x2B3C|
|General Activity Summary Data|0x2B3D|
|Generic Level|0x2AF9|
|GHS Control Point|0x2BF4|
|Global Trade Item Number|0x2AFA|



Bluetooth SIG Proprietary 

Page 88 of 442 

**Assigned Numbers** / Document 

**2026-06-23** 

|Glucose Feature|0x2A51|
|---|---|
|Glucose Measurement|0x2A18|
|Glucose Measurement Context|0x2A34|
|GMAP Role|0x2C00|
|Gust Factor|0x2A74|
|Handedness|0x2B4A|
|Hardware Revision String|0x2A27|
|Health Sensor Features|0x2BF3|
|Hearing Aid Features|0x2BDA|
|Hearing Aid Preset Control Point|0x2BDB|
|Heart Rate Control Point|0x2A39|
|Heart Rate Max|0x2A8D|
|Heart Rate Measurement|0x2A37|
|Heat Index|0x2A7A|
|Height|0x2A8E|
|HID Control Point|0x2A4C|
|HID Information|0x2A4A|
|HID ISO Properties|0x2C23|
|HID SCI Information|0x2C3A|
|HID SCI Mode|0x2C39|
|High Intensity Exercise Threshold|0x2B4D|
|High Resolution Height|0x2B47|
|High Temperature|0x2BDF|
|High Voltage|0x2BE0|
|Hip Circumference|0x2A8F|
|HTTP Control Point|0x2ABA|
|HTTP Entity Body|0x2AB9|
|HTTP Headers|0x2AB7|
|HTTP Status Code|0x2AB8|
|HTTPS Security|0x2ABB|
|Humidity|0x2A6F|
|Humidity 8|0x2C1B|
|IDD Annunciation Status|0x2B22|
|IDD Command Control Point|0x2B25|
|IDD Command Data|0x2B26|
|IDD Features|0x2B23|



Bluetooth SIG Proprietary 

Page 89 of 442 

**Assigned Numbers** / Document 

**2026-06-23** 

|IDD History Data|0x2B28|
|---|---|
|IDD Record Access Control Point|0x2B27|
|IDD Status|0x2B21|
|IDD Status Changed|0x2B20|
|IDD Status Reader Control Point|0x2B24|
|IEEE 11073-20601 Regulatory Certification Data List|0x2A2A|
|Illuminance|0x2AFB|
|Illuminance 16|0x2C1C|
|IMD Control|0x2C12|
|IMD Historical Data|0x2C13|
|IMD Status|0x2C0C|
|IMDS Descriptor Value Changed|0x2C0D|
|Incoming Call|0x2BC1|
|Incoming Call Target Bearer URI|0x2BBC|
|Indoor Bike Data|0x2AD2|
|Indoor Positioning Configuration|0x2AAD|
|Installed Location|0x2C34|
|Intermediate Cuff Pressure|0x2A36|
|Intermediate Temperature|0x2A1E|
|Irradiance|0x2A77|
|Kitchen Appliance Airflow|0x2C30|
|Language|0x2AA2|
|Last Name|0x2A90|
|Latitude|0x2AAE|
|LE GATT Security Levels|0x2BF5|
|LE HID Operation Mode|0x2C24|
|Length|0x2C0A|
|Life Cycle Data|0x2C0F|
|Light Distribution|0x2BE1|
|Light Output|0x2BE2|
|Light Source Type|0x2BE3|
|Linear Position|0x2C08|
|Live Health Observations|0x2B8B|
|LN Control Point|0x2A6B|
|LN Feature|0x2A6A|
|Local East Coordinate|0x2AB1|



Bluetooth SIG Proprietary 

Page 90 of 442 

**Assigned Numbers** / Document 

**2026-06-23** 

|Local North Coordinate|0x2AB0|
|---|---|
|Local Time Information|0x2A0F|
|Location and Speed|0x2A67|
|Location Name|0x2AB5|
|Longitude|0x2AAF|
|Luminous Efficacy|0x2AFC|
|Luminous Energy|0x2AFD|
|Luminous Exposure|0x2AFE|
|Luminous Flux|0x2AFF|
|Luminous Flux Range|0x2B00|
|Luminous Intensity|0x2B01|
|Magnetic Declination|0x2A2C|
|Magnetic Flux Density - 2D|0x2AA0|
|Magnetic Flux Density - 3D|0x2AA1|
|Manufacturer Name String|0x2A29|
|Mass Flow|0x2B02|
|Maximum Recommended Heart Rate|0x2A91|
|Measurement Interval|0x2A21|
|Media Control Point|0x2BA4|
|Media Control Point Opcodes Supported|0x2BA5|
|Media Player Icon Object ID|0x2B94|
|Media Player Icon URL|0x2B95|
|Media Player Name|0x2B93|
|Media State|0x2BA3|
|Mesh Provisioning Data In|0x2ADB|
|Mesh Provisioning Data Out|0x2ADC|
|Mesh Proxy Data In|0x2ADD|
|Mesh Proxy Data Out|0x2ADE|
|Methane Concentration|0x2BD1|
|Middle Name|0x2B48|
|Model Number String|0x2A24|
|Mute|0x2BC3|
|Navigation|0x2A68|
|New Alert|0x2A46|
|Next Track Object ID|0x2B9E|
|Nitrogen Dioxide Concentration<br>Bluetooth SIG Proprietary|0x2BD2|



Page 91 of 442 

**Assigned Numbers** / Document 

**2026-06-23** 

|Noise|0x2BE4|
|---|---|
|Non-Methane Volatile Organic Compounds Concentration|0x2BD3|
|Object Action Control Point|0x2AC5|
|Object Changed|0x2AC8|
|Object First-Created|0x2AC1|
|Object ID|0x2AC3|
|Object Last-Modified|0x2AC2|
|Object List Control Point|0x2AC6|
|Object List Filter|0x2AC7|
|Object Name|0x2ABE|
|Object Properties|0x2AC4|
|Object Size|0x2AC0|
|Object Type|0x2ABF|
|Observation Schedule Changed|0x2BF1|
|On-demand Ranging Data|0x2C16|
|OTS Feature|0x2ABD|
|Ozone Concentration|0x2BD4|
|Parent Group Object ID|0x2B9F|
|Particulate Matter - PM1 Concentration|0x2BD5|
|Particulate Matter - PM10 Concentration|0x2BD7|
|Particulate Matter - PM2.5 Concentration|0x2BD6|
|Perceived Lightness|0x2B03|
|Percentage 8|0x2B04|
|Percentage 8 Steps|0x2C05|
|Peripheral Preferred Connection Parameters|0x2A04|
|Peripheral Privacy Flag|0x2A02|
|Physical Activity Current Session|0x2B44|
|Physical Activity Monitor Control Point|0x2B43|
|Physical Activity Monitor Features|0x2B3B|
|Physical Activity Session Descriptor|0x2B45|
|Playback Speed|0x2B9A|
|Playing Order|0x2BA1|
|Playing Orders Supported|0x2BA2|
|PLX Continuous Measurement|0x2A5F|
|PLX Features|0x2A60|
|PLX Spot-Check Measurement<br>Bluetooth SIG Proprietary|0x2A5E|



Page 92 of 442 

**Assigned Numbers** / Document 

**2026-06-23** 

|PnP ID|0x2A50|
|---|---|
|Pollen Concentration|0x2A75|
|Position Quality|0x2A69|
|Power|0x2B05|
|Power Specification|0x2B06|
|Precise Acceleration - 3D|0x2C1E|
|Preferred Units|0x2B46|
|Pressure|0x2A6D|
|Protocol Mode|0x2A4E|
|Pushbutton Status 8|0x2C21|
|Rainfall|0x2A78|
|Ranging Data Overwritten|0x2C19|
|Ranging Data Ready|0x2C18|
|RAS Control Point|0x2C17|
|RAS Features|0x2C14|
|RC Feature|0x2B1D|
|RC Settings|0x2B1E|
|Real-time Ranging Data|0x2C15|
|Recipe Control|0x2C26|
|Recipe Parameters|0x2C27|
|Reconnection Address|0x2A03|
|Reconnection Configuration Control Point|0x2B1F|
|Record Access Control Point|0x2A52|
|Reference Time Information|0x2A14|
|Registered User|0x2B37|
|Relative Runtime in a Correlated Color Temperature Range|0x2BE5|
|Relative Runtime in a Current Range|0x2B07|
|Relative Runtime in a Generic Level Range|0x2B08|
|Relative Value in a Period of Day|0x2B0B|
|Relative Value in a Temperature Range|0x2B0C|
|Relative Value in a Voltage Range|0x2B09|
|Relative Value in an Illuminance Range|0x2B0A|
|Report|0x2A4D|
|Report Map|0x2A4B|
|Resolvable Private Address Only|0x2AC9|
|Resting Heart Rate<br>Bluetooth SIG Proprietary|0x2A92|



Page 93 of 442 

**Assigned Numbers** / Document 

**2026-06-23** 

|Ringer Control Point|0x2A40|
|---|---|
|Ringer Setting|0x2A41|
|Rotational Speed|0x2C09|
|Rower Data|0x2AD1|
|RSC Feature|0x2A54|
|RSC Measurement|0x2A53|
|SC Control Point|0x2A55|
|Scan Interval Window|0x2A4F|
|Scan Refresh|0x2A31|
|Search Control Point|0x2BA7|
|Search Results Object ID|0x2BA6|
|Sedentary Interval Notification|0x2B4F|
|Seeking Speed|0x2B9B|
|Sensor Location|0x2A5D|
|Serial Number String|0x2A25|
|Server Supported Features|0x2B3A|
|Service Changed|0x2A05|
|Service Cycle Data|0x2C11|
|Set Identity Resolving Key|0x2B84|
|Set Member Lock|0x2B86|
|Set Member Rank|0x2B87|
|Sink ASE|0x2BC4|
|Sink Audio Locations|0x2BCA|
|Sink PAC|0x2BC9|
|Sleep Activity Instantaneous Data|0x2B41|
|Sleep Activity Summary Data|0x2B42|
|Software Revision String|0x2A28|
|Source ASE|0x2BC5|
|Source Audio Locations|0x2BCC|
|Source PAC|0x2BCB|
|Sport Type for Aerobic and Anaerobic Thresholds|0x2A93|
|Stair Climber Data|0x2AD0|
|Status Flags|0x2BBB|
|Step Climber Data|0x2ACF|
|Step Counter Activity Summary Data|0x2B40|
|Stored Health Observations|0x2BDD|



Bluetooth SIG Proprietary 

Page 94 of 442 

**Assigned Numbers** / Document 

**2026-06-23** 

|Stride Length|0x2B49|
|---|---|
|Sulfur Dioxide Concentration|0x2BD8|
|Sulfur Hexafluoride Concentration|0x2BD9|
|Supported Audio Contexts|0x2BCE|
|Supported Heart Rate Range|0x2AD7|
|Supported Inclination Range|0x2AD5|
|Supported New Alert Category|0x2A47|
|Supported Power Range|0x2AD8|
|Supported Resistance Level Range|0x2AD6|
|Supported Speed Range|0x2AD4|
|Supported Unread Alert Category|0x2A48|
|System ID|0x2A23|
|TDS Control Point|0x2ABC|
|Temperature|0x2A6E|
|Temperature 8|0x2B0D|
|Temperature 8 in a Period of Day|0x2B0E|
|Temperature 8 Statistics|0x2B0F|
|Temperature Measurement|0x2A1C|
|Temperature Range|0x2B10|
|Temperature Statistics|0x2B11|
|Temperature Type|0x2A1D|
|Termination Reason|0x2BC0|
|Three Zone Heart Rate Limits|0x2A94|
|Time Accuracy|0x2A12|
|Time Change Log Data|0x2B92|
|Time Decihour 8|0x2B12|
|Time Exponential 8|0x2B13|
|Time Hour 24|0x2B14|
|Time Millisecond 24|0x2B15|
|Time Second 16|0x2B16|
|Time Second 32|0x2BE6|
|Time Second 8|0x2B17|
|Time Source|0x2A13|
|Time Update Control Point|0x2A16|
|Time Update State|0x2A17|
|Time with DST|0x2A11|



Bluetooth SIG Proprietary 

Page 95 of 442 

**Assigned Numbers** / Document 

**2026-06-23** 

|Time Zone|0x2A0E|
|---|---|
|TMAP Role|0x2B51|
|Torque|0x2C0B|
|Track Changed|0x2B96|
|Track Duration|0x2B98|
|Track Position|0x2B99|
|Track Title|0x2B97|
|Training Status|0x2AD3|
|Treadmill Data|0x2ACD|
|True Wind Direction|0x2A71|
|True Wind Speed|0x2A70|
|Two Zone Heart Rate Limits|0x2A95|
|Tx Power Level|0x2A07|
|UDI for Medical Devices|0x2BFF|
|UGG Features|0x2C01|
|UGT Features|0x2C02|
|Uncertainty|0x2AB4|
|Unread Alert Status|0x2A45|
|URI|0x2AB6|
|User Control Point|0x2A9F|
|User Index|0x2A9A|
|UV Index|0x2A76|
|VO2 Max|0x2A96|
|VOC Concentration|0x2BE7|
|Voice Assistant Name|0x2C31|
|Voice Assistant Service Control Point|0x2C33|
|Voice Assistant Session Flag|0x2C36|
|Voice Assistant Session State|0x2C35|
|Voice Assistant Supported Features|0x2C38|
|Voice Assistant Supported Languages|0x2C37|
|Voice Assistant UUID|0x2C32|
|Voltage|0x2B18|
|Voltage Frequency|0x2BE8|
|Voltage Specification|0x2B19|
|Voltage Statistics|0x2B1A|
|Volume Control Point|0x2B7E|



Bluetooth SIG Proprietary 

Page 96 of 442 

**Assigned Numbers** / Document 

**2026-06-23** 

|Volume Flags|0x2B7F|
|---|---|
|Volume Flow|0x2B1B|
|Volume Offset Control Point|0x2B82|
|Volume Offset State|0x2B80|
|Volume State|0x2B7D|
|Waist Circumference|0x2A97|
|Weight|0x2A98|
|Weight Measurement|0x2A9D|
|Weight Scale Feature|0x2A9E|
|Wind Chill|0x2A79|
|Work Cycle Data|0x2C10|



Bluetooth SIG Proprietary 

Page 97 of 442 

**Assigned Numbers** / Document 

**2026-06-23** 

##### **3.8.2 Characteristics by UUID** 

###### Last Modified: 2026-04-23 

Filename: assigned_numbers/uuids/characteristic_uuids.yaml 

|**UUID**|**Characteristic Name**|
|---|---|
|0x2A00|Device Name|
|0x2A01|Appearance|
|0x2A02|Peripheral Privacy Flag|
|0x2A03|Reconnection Address|
|0x2A04|Peripheral Preferred Connection Parameters|
|0x2A05|Service Changed|
|0x2A06|Alert Level|
|0x2A07|Tx Power Level|
|0x2A08|Date Time|
|0x2A09|Day of Week|
|0x2A0A|Day Date Time|
|0x2A0C|Exact Time 256|
|0x2A0D|DST Offset|
|0x2A0E|Time Zone|
|0x2A0F|Local Time Information|
|0x2A11|Time with DST|
|0x2A12|Time Accuracy|
|0x2A13|Time Source|
|0x2A14|Reference Time Information|
|0x2A16|Time Update Control Point|
|0x2A17|Time Update State|
|0x2A18|Glucose Measurement|
|0x2A19|Battery Level|
|0x2A1C|Temperature Measurement|
|0x2A1D|Temperature Type|
|0x2A1E|Intermediate Temperature|
|0x2A21|Measurement Interval|
|0x2A22|Boot Keyboard Input Report|
|0x2A23|System ID|
|0x2A24|Model Number String|
|0x2A25|Serial Number String|



Bluetooth SIG Proprietary 

Page 98 of 442 

**Assigned Numbers** / Document 

**2026-06-23** 

|0x2A26|Firmware Revision String|
|---|---|
|0x2A27|Hardware Revision String|
|0x2A28|Software Revision String|
|0x2A29|Manufacturer Name String|
|0x2A2A|IEEE 11073-20601 Regulatory Certification Data List|
|0x2A2B|Current Time|
|0x2A2C|Magnetic Declination|
|0x2A31|Scan Refresh|
|0x2A32|Boot Keyboard Output Report|
|0x2A33|Boot Mouse Input Report|
|0x2A34|Glucose Measurement Context|
|0x2A35|Blood Pressure Measurement|
|0x2A36|Intermediate Cuff Pressure|
|0x2A37|Heart Rate Measurement|
|0x2A38|Body Sensor Location|
|0x2A39|Heart Rate Control Point|
|0x2A3F|Alert Status|
|0x2A40|Ringer Control Point|
|0x2A41|Ringer Setting|
|0x2A42|Alert Category ID Bit Mask|
|0x2A43|Alert Category ID|
|0x2A44|Alert Notification Control Point|
|0x2A45|Unread Alert Status|
|0x2A46|New Alert|
|0x2A47|Supported New Alert Category|
|0x2A48|Supported Unread Alert Category|
|0x2A49|Blood Pressure Feature|
|0x2A4A|HID Information|
|0x2A4B|Report Map|
|0x2A4C|HID Control Point|
|0x2A4D|Report|
|0x2A4E|Protocol Mode|
|0x2A4F|Scan Interval Window|
|0x2A50|PnP ID|
|0x2A51|Glucose Feature|
|0x2A52|Record Access Control Point|



Bluetooth SIG Proprietary 

Page 99 of 442 

**Assigned Numbers** / Document 

**2026-06-23** 

|0x2A53|RSC Measurement|
|---|---|
|0x2A54|RSC Feature|
|0x2A55|SC Control Point|
|0x2A5A|Aggregate|
|0x2A5B|CSC Measurement|
|0x2A5C|CSC Feature|
|0x2A5D|Sensor Location|
|0x2A5E|PLX Spot-Check Measurement|
|0x2A5F|PLX Continuous Measurement|
|0x2A60|PLX Features|
|0x2A63|Cycling Power Measurement|
|0x2A64|Cycling Power Vector|
|0x2A65|Cycling Power Feature|
|0x2A66|Cycling Power Control Point|
|0x2A67|Location and Speed|
|0x2A68|Navigation|
|0x2A69|Position Quality|
|0x2A6A|LN Feature|
|0x2A6B|LN Control Point|
|0x2A6C|Elevation|
|0x2A6D|Pressure|
|0x2A6E|Temperature|
|0x2A6F|Humidity|
|0x2A70|True Wind Speed|
|0x2A71|True Wind Direction|
|0x2A72|Apparent Wind Speed|
|0x2A73|Apparent Wind Direction|
|0x2A74|Gust Factor|
|0x2A75|Pollen Concentration|
|0x2A76|UV Index|
|0x2A77|Irradiance|
|0x2A78|Rainfall|
|0x2A79|Wind Chill|
|0x2A7A|Heat Index|
|0x2A7B|Dew Point|
|0x2A7D|Descriptor Value Changed|



Bluetooth SIG Proprietary 

Page 100 of 442 

**Assigned Numbers** / Document 

**2026-06-23** 

|0x2A7E|Aerobic Heart Rate Lower Limit|
|---|---|
|0x2A7F|Aerobic Threshold|
|0x2A80|Age|
|0x2A81|Anaerobic Heart Rate Lower Limit|
|0x2A82|Anaerobic Heart Rate Upper Limit|
|0x2A83|Anaerobic Threshold|
|0x2A84|Aerobic Heart Rate Upper Limit|
|0x2A85|Date of Birth|
|0x2A86|Date of Threshold Assessment|
|0x2A87|Email Address|
|0x2A88|Fat Burn Heart Rate Lower Limit|
|0x2A89|Fat Burn Heart Rate Upper Limit|
|0x2A8A|First Name|
|0x2A8B|Five Zone Heart Rate Limits|
|0x2A8C|Gender|
|0x2A8D|Heart Rate Max|
|0x2A8E|Height|
|0x2A8F|Hip Circumference|
|0x2A90|Last Name|
|0x2A91|Maximum Recommended Heart Rate|
|0x2A92|Resting Heart Rate|
|0x2A93|Sport Type for Aerobic and Anaerobic Thresholds|
|0x2A94|Three Zone Heart Rate Limits|
|0x2A95|Two Zone Heart Rate Limits|
|0x2A96|VO2 Max|
|0x2A97|Waist Circumference|
|0x2A98|Weight|
|0x2A99|Database Change Increment|
|0x2A9A|User Index|
|0x2A9B|Body Composition Feature|
|0x2A9C|Body Composition Measurement|
|0x2A9D|Weight Measurement|
|0x2A9E|Weight Scale Feature|
|0x2A9F|User Control Point|
|0x2AA0|Magnetic Flux Density - 2D|
|0x2AA1|Magnetic Flux Density - 3D|



Bluetooth SIG Proprietary 

Page 101 of 442 

**Assigned Numbers** / Document 

**2026-06-23** 

|0x2AA2|Language|
|---|---|
|0x2AA3|Barometric Pressure Trend|
|0x2AA4|Bond Management Control Point|
|0x2AA5|Bond Management Feature|
|0x2AA6|Central Address Resolution|
|0x2AA7|CGM Measurement|
|0x2AA8|CGM Feature|
|0x2AA9|CGM Status|
|0x2AAA|CGM Session Start Time|
|0x2AAB|CGM Session Run Time|
|0x2AAC|CGM Specific Ops Control Point|
|0x2AAD|Indoor Positioning Configuration|
|0x2AAE|Latitude|
|0x2AAF|Longitude|
|0x2AB0|Local North Coordinate|
|0x2AB1|Local East Coordinate|
|0x2AB2|Floor Number|
|0x2AB3|Altitude|
|0x2AB4|Uncertainty|
|0x2AB5|Location Name|
|0x2AB6|URI|
|0x2AB7|HTTP Headers|
|0x2AB8|HTTP Status Code|
|0x2AB9|HTTP Entity Body|
|0x2ABA|HTTP Control Point|
|0x2ABB|HTTPS Security|
|0x2ABC|TDS Control Point|
|0x2ABD|OTS Feature|
|0x2ABE|Object Name|
|0x2ABF|Object Type|
|0x2AC0|Object Size|
|0x2AC1|Object First-Created|
|0x2AC2|Object Last-Modified|
|0x2AC3|Object ID|
|0x2AC4|Object Properties|
|0x2AC5|Object Action Control Point|



Bluetooth SIG Proprietary 

Page 102 of 442 

**Assigned Numbers** / Document 

**2026-06-23** 

|0x2AC6|Object List Control Point|
|---|---|
|0x2AC7|Object List Filter|
|0x2AC8|Object Changed|
|0x2AC9|Resolvable Private Address Only|
|0x2ACC|Fitness Machine Feature|
|0x2ACD|Treadmill Data|
|0x2ACE|Cross Trainer Data|
|0x2ACF|Step Climber Data|
|0x2AD0|Stair Climber Data|
|0x2AD1|Rower Data|
|0x2AD2|Indoor Bike Data|
|0x2AD3|Training Status|
|0x2AD4|Supported Speed Range|
|0x2AD5|Supported Inclination Range|
|0x2AD6|Supported Resistance Level Range|
|0x2AD7|Supported Heart Rate Range|
|0x2AD8|Supported Power Range|
|0x2AD9|Fitness Machine Control Point|
|0x2ADA|Fitness Machine Status|
|0x2ADB|Mesh Provisioning Data In|
|0x2ADC|Mesh Provisioning Data Out|
|0x2ADD|Mesh Proxy Data In|
|0x2ADE|Mesh Proxy Data Out|
|0x2AE0|Average Current|
|0x2AE1|Average Voltage|
|0x2AE2|Boolean|
|0x2AE3|Chromatic Distance from Planckian|
|0x2AE4|Chromaticity Coordinates|
|0x2AE5|Chromaticity in CCT and Duv Values|
|0x2AE6|Chromaticity Tolerance|
|0x2AE7|CIE 13.3-1995 Color Rendering Index|
|0x2AE8|Coefficient|
|0x2AE9|Correlated Color Temperature|
|0x2AEA|Count 16|
|0x2AEB|Count 24|
|0x2AEC|Country Code|



Bluetooth SIG Proprietary 

Page 103 of 442 

**Assigned Numbers** / Document 

**2026-06-23** 

|0x2AED|Date UTC|
|---|---|
|0x2AEE|Electric Current|
|0x2AEF|Electric Current Range|
|0x2AF0|Electric Current Specification|
|0x2AF1|Electric Current Statistics|
|0x2AF2|Energy|
|0x2AF3|Energy in a Period of Day|
|0x2AF4|Event Statistics|
|0x2AF5|Fixed String 16|
|0x2AF6|Fixed String 24|
|0x2AF7|Fixed String 36|
|0x2AF8|Fixed String 8|
|0x2AF9|Generic Level|
|0x2AFA|Global Trade Item Number|
|0x2AFB|Illuminance|
|0x2AFC|Luminous Efficacy|
|0x2AFD|Luminous Energy|
|0x2AFE|Luminous Exposure|
|0x2AFF|Luminous Flux|
|0x2B00|Luminous Flux Range|
|0x2B01|Luminous Intensity|
|0x2B02|Mass Flow|
|0x2B03|Perceived Lightness|
|0x2B04|Percentage 8|
|0x2B05|Power|
|0x2B06|Power Specification|
|0x2B07|Relative Runtime in a Current Range|
|0x2B08|Relative Runtime in a Generic Level Range|
|0x2B09|Relative Value in a Voltage Range|
|0x2B0A|Relative Value in an Illuminance Range|
|0x2B0B|Relative Value in a Period of Day|
|0x2B0C|Relative Value in a Temperature Range|
|0x2B0D|Temperature 8|
|0x2B0E|Temperature 8 in a Period of Day|
|0x2B0F|Temperature 8 Statistics|
|0x2B10|Temperature Range|



Bluetooth SIG Proprietary 

Page 104 of 442 

**Assigned Numbers** / Document 

**2026-06-23** 

|0x2B11|Temperature Statistics|
|---|---|
|0x2B12|Time Decihour 8|
|0x2B13|Time Exponential 8|
|0x2B14|Time Hour 24|
|0x2B15|Time Millisecond 24|
|0x2B16|Time Second 16|
|0x2B17|Time Second 8|
|0x2B18|Voltage|
|0x2B19|Voltage Specification|
|0x2B1A|Voltage Statistics|
|0x2B1B|Volume Flow|
|0x2B1C|Chromaticity Coordinate|
|0x2B1D|RC Feature|
|0x2B1E|RC Settings|
|0x2B1F|Reconnection Configuration Control Point|
|0x2B20|IDD Status Changed|
|0x2B21|IDD Status|
|0x2B22|IDD Annunciation Status|
|0x2B23|IDD Features|
|0x2B24|IDD Status Reader Control Point|
|0x2B25|IDD Command Control Point|
|0x2B26|IDD Command Data|
|0x2B27|IDD Record Access Control Point|
|0x2B28|IDD History Data|
|0x2B29|Client Supported Features|
|0x2B2A|Database Hash|
|0x2B2B|BSS Control Point|
|0x2B2C|BSS Response|
|0x2B2D|Emergency ID|
|0x2B2E|Emergency Text|
|0x2B2F|ACS Status|
|0x2B30|ACS Data In|
|0x2B31|ACS Data Out Notify|
|0x2B32|ACS Data Out Indicate|
|0x2B33|ACS Control Point|
|0x2B34|Enhanced Blood Pressure Measurement|



Bluetooth SIG Proprietary 

Page 105 of 442 

**Assigned Numbers** / Document 

**2026-06-23** 

|0x2B35|Enhanced Intermediate Cuff Pressure|
|---|---|
|0x2B36|Blood Pressure Record|
|0x2B37|Registered User|
|0x2B38|BR-EDR Handover Data|
|0x2B39|Bluetooth SIG Data|
|0x2B3A|Server Supported Features|
|0x2B3B|Physical Activity Monitor Features|
|0x2B3C|General Activity Instantaneous Data|
|0x2B3D|General Activity Summary Data|
|0x2B3E|CardioRespiratory Activity Instantaneous Data|
|0x2B3F|CardioRespiratory Activity Summary Data|
|0x2B40|Step Counter Activity Summary Data|
|0x2B41|Sleep Activity Instantaneous Data|
|0x2B42|Sleep Activity Summary Data|
|0x2B43|Physical Activity Monitor Control Point|
|0x2B44|Physical Activity Current Session|
|0x2B45|Physical Activity Session Descriptor|
|0x2B46|Preferred Units|
|0x2B47|High Resolution Height|
|0x2B48|Middle Name|
|0x2B49|Stride Length|
|0x2B4A|Handedness|
|0x2B4B|Device Wearing Position|
|0x2B4C|Four Zone Heart Rate Limits|
|0x2B4D|High Intensity Exercise Threshold|
|0x2B4E|Activity Goal|
|0x2B4F|Sedentary Interval Notification|
|0x2B50|Caloric Intake|
|0x2B51|TMAP Role|
|0x2B77|Audio Input State|
|0x2B78|Gain Settings Attribute|
|0x2B79|Audio Input Type|
|0x2B7A|Audio Input Status|
|0x2B7B|Audio Input Control Point|
|0x2B7C|Audio Input Description|
|0x2B7D|Volume State|



Bluetooth SIG Proprietary 

Page 106 of 442 

**Assigned Numbers** / Document 

**2026-06-23** 

|0x2B7E|Volume Control Point|
|---|---|
|0x2B7F|Volume Flags|
|0x2B80|Volume Offset State|
|0x2B81|Audio Location|
|0x2B82|Volume Offset Control Point|
|0x2B83|Audio Output Description|
|0x2B84|Set Identity Resolving Key|
|0x2B85|Coordinated Set Size|
|0x2B86|Set Member Lock|
|0x2B87|Set Member Rank|
|0x2B88|Encrypted Data Key Material|
|0x2B89|Apparent Energy 32|
|0x2B8A|Apparent Power|
|0x2B8B|Live Health Observations|
|0x2B8C|CO2 Concentration|
|0x2B8D|Cosine of the Angle|
|0x2B8E|Device Time Feature|
|0x2B8F|Device Time Parameters|
|0x2B90|Device Time|
|0x2B91|Device Time Control Point|
|0x2B92|Time Change Log Data|
|0x2B93|Media Player Name|
|0x2B94|Media Player Icon Object ID|
|0x2B95|Media Player Icon URL|
|0x2B96|Track Changed|
|0x2B97|Track Title|
|0x2B98|Track Duration|
|0x2B99|Track Position|
|0x2B9A|Playback Speed|
|0x2B9B|Seeking Speed|
|0x2B9C|Current Track Segments Object ID|
|0x2B9D|Current Track Object ID|
|0x2B9E|Next Track Object ID|
|0x2B9F|Parent Group Object ID|
|0x2BA0|Current Group Object ID|
|0x2BA1|Playing Order|



Bluetooth SIG Proprietary 

Page 107 of 442 

**Assigned Numbers** / Document 

**2026-06-23** 

|0x2BA2|Playing Orders Supported|
|---|---|
|0x2BA3|Media State|
|0x2BA4|Media Control Point|
|0x2BA5|Media Control Point Opcodes Supported|
|0x2BA6|Search Results Object ID|
|0x2BA7|Search Control Point|
|0x2BA8|Energy 32|
|0x2BAD|Constant Tone Extension Enable|
|0x2BAE|Advertising Constant Tone Extension Minimum Length|
|0x2BAF|Advertising Constant Tone Extension Minimum Transmit Count|
|0x2BB0|Advertising Constant Tone Extension Transmit Duration|
|0x2BB1|Advertising Constant Tone Extension Interval|
|0x2BB2|Advertising Constant Tone Extension PHY|
|0x2BB3|Bearer Provider Name|
|0x2BB4|Bearer UCI|
|0x2BB5|Bearer Technology|
|0x2BB6|Bearer URI Schemes Supported List|
|0x2BB7|Bearer Signal Strength|
|0x2BB8|Bearer Signal Strength Reporting Interval|
|0x2BB9|Bearer List Current Calls|
|0x2BBA|Content Control ID|
|0x2BBB|Status Flags|
|0x2BBC|Incoming Call Target Bearer URI|
|0x2BBD|Call State|
|0x2BBE|Call Control Point|
|0x2BBF|Call Control Point Optional Opcodes|
|0x2BC0|Termination Reason|
|0x2BC1|Incoming Call|
|0x2BC2|Call Friendly Name|
|0x2BC3|Mute|
|0x2BC4|Sink ASE|
|0x2BC5|Source ASE|
|0x2BC6|ASE Control Point|
|0x2BC7|Broadcast Audio Scan Control Point|
|0x2BC8|Broadcast Receive State|
|0x2BC9|Sink PAC|



Bluetooth SIG Proprietary 

Page 108 of 442 

**Assigned Numbers** / Document 

**2026-06-23** 

|0x2BCA|Sink Audio Locations|
|---|---|
|0x2BCB|Source PAC|
|0x2BCC|Source Audio Locations|
|0x2BCD|Available Audio Contexts|
|0x2BCE|Supported Audio Contexts|
|0x2BCF|Ammonia Concentration|
|0x2BD0|Carbon Monoxide Concentration|
|0x2BD1|Methane Concentration|
|0x2BD2|Nitrogen Dioxide Concentration|
|0x2BD3|Non-Methane Volatile Organic Compounds Concentration|
|0x2BD4|Ozone Concentration|
|0x2BD5|Particulate Matter - PM1 Concentration|
|0x2BD6|Particulate Matter - PM2.5 Concentration|
|0x2BD7|Particulate Matter - PM10 Concentration|
|0x2BD8|Sulfur Dioxide Concentration|
|0x2BD9|Sulfur Hexafluoride Concentration|
|0x2BDA|Hearing Aid Features|
|0x2BDB|Hearing Aid Preset Control Point|
|0x2BDC|Active Preset Index|
|0x2BDD|Stored Health Observations|
|0x2BDE|Fixed String 64|
|0x2BDF|High Temperature|
|0x2BE0|High Voltage|
|0x2BE1|Light Distribution|
|0x2BE2|Light Output|
|0x2BE3|Light Source Type|
|0x2BE4|Noise|
|0x2BE5|Relative Runtime in a Correlated Color Temperature Range|
|0x2BE6|Time Second 32|
|0x2BE7|VOC Concentration|
|0x2BE8|Voltage Frequency|
|0x2BE9|Battery Critical Status|
|0x2BEA|Battery Health Status|
|0x2BEB|Battery Health Information|
|0x2BEC|Battery Information|
|0x2BED|Battery Level Status|



Bluetooth SIG Proprietary 

Page 109 of 442 

**Assigned Numbers** / Document 

**2026-06-23** 

|0x2BEE|Battery Time Status|
|---|---|
|0x2BEF|Estimated Service Date|
|0x2BF0|Battery Energy Status|
|0x2BF1|Observation Schedule Changed|
|0x2BF2|Elapsed Time|
|0x2BF3|Health Sensor Features|
|0x2BF4|GHS Control Point|
|0x2BF5|LE GATT Security Levels|
|0x2BF6|ESL Address|
|0x2BF7|AP Sync Key Material|
|0x2BF8|ESL Response Key Material|
|0x2BF9|ESL Current Absolute Time|
|0x2BFA|ESL Display Information|
|0x2BFB|ESL Image Information|
|0x2BFC|ESL Sensor Information|
|0x2BFD|ESL LED Information|
|0x2BFE|ESL Control Point|
|0x2BFF|UDI for Medical Devices|
|0x2C00|GMAP Role|
|0x2C01|UGG Features|
|0x2C02|UGT Features|
|0x2C03|BGS Features|
|0x2C04|BGR Features|
|0x2C05|Percentage 8 Steps|
|0x2C06|Acceleration|
|0x2C07|Force|
|0x2C08|Linear Position|
|0x2C09|Rotational Speed|
|0x2C0A|Length|
|0x2C0B|Torque|
|0x2C0C|IMD Status|
|0x2C0D|IMDS Descriptor Value Changed|
|0x2C0E|First Use Date|
|0x2C0F|Life Cycle Data|
|0x2C10|Work Cycle Data|
|0x2C11|Service Cycle Data|



Bluetooth SIG Proprietary 

Page 110 of 442 

**Assigned Numbers** / Document 

**2026-06-23** 

|0x2C12|IMD Control|
|---|---|
|0x2C13|IMD Historical Data|
|0x2C14|RAS Features|
|0x2C15|Real-time Ranging Data|
|0x2C16|On-demand Ranging Data|
|0x2C17|RAS Control Point|
|0x2C18|Ranging Data Ready|
|0x2C19|Ranging Data Overwritten|
|0x2C1A|Coordinated Set Name|
|0x2C1B|Humidity 8|
|0x2C1C|Illuminance 16|
|0x2C1D|Acceleration - 3D|
|0x2C1E|Precise Acceleration - 3D|
|0x2C1F|Acceleration Detection Status|
|0x2C20|Door/Window Status|
|0x2C21|Pushbutton Status 8|
|0x2C22|Contact Status 8|
|0x2C23|HID ISO Properties|
|0x2C24|LE HID Operation Mode|
|0x2C25|Cookware Description|
|0x2C26|Recipe Control|
|0x2C27|Recipe Parameters|
|0x2C28|Cooking Step Status|
|0x2C29|Cooking Zone Capabilities|
|0x2C2A|Cooking Zone Desired Cooking Conditions|
|0x2C2B|Cooking Zone Actual Cooking Conditions|
|0x2C2C|Cookware Sensor Data|
|0x2C2D|Cookware Sensor Aggregate|
|0x2C2E|Cooking Temperature|
|0x2C2F|Cooking Zone Perceived Power|
|0x2C30|Kitchen Appliance Airflow|
|0x2C31|Voice Assistant Name|
|0x2C32|Voice Assistant UUID|
|0x2C33|Voice Assistant Service Control Point|
|0x2C34|Installed Location|
|0x2C35|Voice Assistant Session State|



Bluetooth SIG Proprietary 

Page 111 of 442 

**Assigned Numbers** / Document 

**2026-06-23** 

|0x2C36|Voice Assistant Session Flag|
|---|---|
|0x2C37|Voice Assistant Supported Languages|
|0x2C38|Voice Assistant Supported Features|
|0x2C39|HID SCI Mode|
|0x2C3A|HID SCI Information|



Bluetooth SIG Proprietary 

Page 112 of 442 

**Assigned Numbers** / Document 

**2026-06-23** 

#### **3.9 Object Types** 

##### **3.9.1 Object Types by Name** 

###### Last Modified: 2024-10-18 

Filename: assigned_numbers/uuids/object_types.yaml 

|**Object Type Name**|**UUID**|
|---|---|
|Directory Listing|0x2ACB|
|Group|0x2BAC|
|Media Player Icon|0x2BA9|
|Track|0x2BAB|
|Track Segment|0x2BAA|
|Unspecified|0x2ACA|



Bluetooth SIG Proprietary 

Page 113 of 442 

**Assigned Numbers** / Document 

**2026-06-23** 

##### **3.9.2 Object Types by UUID** 

Last Modified: 2024-10-18 

Filename: assigned_numbers/uuids/object_types.yaml 

|**UUID**|**Object Type Name**|
|---|---|
|0x2ACA|Unspecified|
|0x2ACB|Directory Listing|
|0x2BA9|Media Player Icon|
|0x2BAA|Track Segment|
|0x2BAB|Track|
|0x2BAC|Group|



Bluetooth SIG Proprietary 

Page 114 of 442 

**Assigned Numbers** / Document 

**2026-06-23** 

#### **3.10 SDO Services** 

Last Modified: 2024-05-30 

Filename: assigned_numbers/uuids/sdo_uuids.yaml 

|**UUID**|**Name**|**SDO Name**|
|---|---|---|
|0xFCCC|Wi-Fi Easy Connect Specification Service|Wi-Fi Alliance|
|0xFFEF|Wi-Fi Direct Specification Service|Wi-Fi Alliance|
|0xFFF0|Public Key Open Credential (PKOC)<br>Service|Physical Security Interoperability Alliance|
|0xFFF1|ICCE Digital Key Service|China Association of Automobile<br>Manufactures|
|0xFFF2|Aliro Service|Connectivity Standards Alliance|
|0xFFF3|FiRa Consortium Service|FiRa Consortium|
|0xFFF4|FiRa Consortium Service|FiRa Consortium|
|0xFFF5|Car Connectivity Consortium, LLC<br>Service|Car Connectivity Consortium, LLC|
|0xFFF6|Matter Profile ID Service|Connectivity Standards Alliance|
|0xFFF7|Zigbee Direct Service|Connectivity Standards Alliance|
|0xFFF8|Mopria Alliance BLE Service|Mopria Alliance|
|0xFFF9|FIDO2 secure client-to-authenticator<br>transport Service|Fast IDentity Online Alliance (FIDO)|
|0xFFFA|ASTM Remote ID Service|ASTM International|
|0xFFFB|Direct Thread Commissioning Service|Thread Group, Inc.|
|0xFFFC|Wireless Power Transfer (WPT) Service|AirFuel Alliance|
|0xFFFD|Universal Second Factor Authenticator<br>Service|Fast IDentity Online Alliance (FIDO)|
|0xFFFE|Wireless Power Transfer Service|AirFuel Alliance (formerly Alliance for<br>Wireless Power)|



Bluetooth SIG Proprietary 

Page 115 of 442 

**Assigned Numbers** / Document 

**2026-06-23** 

#### **3.11 Member Services** 

The table lists the Member Service UUIDs allocated to member companies. The table lists only the name of the member company that requested the UUID and not the name of the service. 

To request a new Member Service UUID, please submit a ticket to Bluetooth Support and select the Assigned Numbers category (login required). For those not familiar with the assignment process, please refer to Section 2.3 of the Assigned Numbers Process Document which is available on the Templates and Documents page. 

Please allow five business days for your request to be fulfilled, and another five business days from the time your request is fulfilled to view your Member Service UUID on this page. 

Last Modified: 2026-06-16 

Filename: assigned_numbers/uuids/member_uuids.yaml 

|**UUID**|**Member Company**|
|---|---|
|0xFC30|Arashi Vision Inc.|
|0xFC31|Ford Motor Company|
|0xFC32|InPlay, Inc.|
|0xFC34|Mitsubishi Electric Corporation|
|0xFC35|Metabowerke GmbH|
|0xFC36|PharmaSens AG|
|0xFC37|TANlock GmbH|
|0xFC38|SetPoint Medical|
|0xFC39|Harman International|
|0xFC3A|C.O.B.O. SpA|
|0xFC3B|BONX INC.|
|0xFC3C|Eforthink Technology Co., Ltd.|
|0xFC3D|Reelables, Inc.|
|0xFC3E|Google LLC|
|0xFC3F|Milwaukee Electric Tools|
|0xFC41|Yoto Limited|
|0xFC43|Atmosic Technologies, Inc.|
|0xFC44|Block, Inc.|
|0xFC45|Mitsubishi Electric Corporation|
|0xFC46|Xiaomi|
|0xFC47|Shanghai Ingeek Technology Co., Ltd.|
|0xFC48|Michelin|
|0xFC49|Golioth, Inc.|
|0xFC4A|Shenzhen Shokz Co.,Ltd.|
|0xFC4B|WinMagic Inc.|



Bluetooth SIG Proprietary 

Page 116 of 442 

**Assigned Numbers** / Document 

**2026-06-23** 

|0xFC4C|HP Inc.|
|---|---|
|0xFC4D|Lodestar Technology Inc.|
|0xFC4E|Lodestar Technology Inc.|
|0xFC4F|WaveRF, Corp.|
|0xFC50|Ant Group Co., Ltd.|
|0xFC51|Ant Group Co., Ltd.|
|0xFC52|LG Electronics Inc.|
|0xFC53|LEGIC Identsystems AG|
|0xFC54|Shenzhen Yinwang Intelligent Technologies Co., Ltd.|
|0xFC55|BYD Company Limited|
|0xFC56|Google LLC|
|0xFC57|Ambient Life Inc.|
|0xFC58|Shenzhen Minew Technologies Co., Ltd.|
|0xFC59|Ant Group Co., Ltd.|
|0xFC5A|LAST LOCK INC.|
|0xFC5B|Time Location Systems AS|
|0xFC5C|PLASTIC RESEARCH AND DEVELOPMENT CORPORATION|
|0xFC5D|GP Acoustics International Limited|
|0xFC5E|KUBU SMART LIMITED|
|0xFC5F|PI-CRYSTAL INC.|
|0xFC60|Ohme Operations UK Limited|
|0xFC61|QIKCONNEX LLC|
|0xFC62|SPRiNTUS GmbH|
|0xFC63|Volvo Technology AB|
|0xFC64|Volvo Technology AB|
|0xFC65|Robor Electronics B.V.|
|0xFC66|Xiaomi Inc.|
|0xFC67|Guangdong Hengqin Xingtong Technology Co.,ltd.|
|0xFC68|RIGH, INC.|
|0xFC69|Harman International|
|0xFC6A|Sonos Inc|
|0xFC6B|Sonos Inc|
|0xFC6C|Powerstick.com|
|0xFC6D|MOTIVE TECHNOLOGIES, INC.|
|0xFC6E|stryker|
|0xFC6F|NextSense, Inc.|



Bluetooth SIG Proprietary 

Page 117 of 442 

**Assigned Numbers** / Document 

**2026-06-23** 

|0xFC70|MOTIVE TECHNOLOGIES, INC.|
|---|---|
|0xFC71|Hive-Zox International SA|
|0xFC72|iodyne, LLC|
|0xFC73|Google LLC|
|0xFC74|EMBEINT INC|
|0xFC75|Xiaomi Inc.|
|0xFC76|Weber-Stephen Products LLC|
|0xFC77|SING SUN TECHNOLOGY (INTERNATIONAL) LIMITED|
|0xFC78|DHL|
|0xFC79|LG Electronics Inc.|
|0xFC7A|Outshiny India Private Limited|
|0xFC7B|Testo SE & Co. KGaA|
|0xFC7C|Motorola Mobility, LLC|
|0xFC7D|MML US, Inc|
|0xFC7E|Harman International|
|0xFC7F|Southco|
|0xFC80|TELE System Communications Pte. Ltd.|
|0xFC81|Axon Enterprise, Inc.|
|0xFC82|Zwift, Inc.|
|0xFC83|iHealth Labs, Inc.|
|0xFC84|NINGBO FOTILE KITCHENWARE CO., LTD.|
|0xFC85|Zhejiang Huanfu Technology Co., LTD|
|0xFC86|Samsara Networks, Inc|
|0xFC87|Samsara Networks, Inc|
|0xFC88|CCC del Uruguay|
|0xFC89|Intel Corporation|
|0xFC8A|Intel Corporation|
|0xFC8B|Kaspersky Lab Middle East FZ-LLC|
|0xFC8C|VusionGroup|
|0xFC8D|Caire Inc.|
|0xFC8E|Blue Iris Labs, Inc.|
|0xFC8F|Bose Corporation|
|0xFC90|Wiliot LTD.|
|0xFC91|Samsung Electronics Co., Ltd.|
|0xFC92|Furuno Electric Co., Ltd.|
|0xFC93|Komatsu Ltd.|



Bluetooth SIG Proprietary 

Page 118 of 442 

**Assigned Numbers** / Document 

**2026-06-23** 

|0xFC94|Apple Inc.|
|---|---|
|0xFC95|Hippo Camp Software Ltd.|
|0xFC96|LEGO System A/S|
|0xFC97|Japan Display Inc.|
|0xFC98|Ruuvi Innovations Ltd.|
|0xFC99|Badger Meter|
|0xFC9A|Koppli AB|
|0xFC9B|Merry Electronics (S) Pte Ltd|
|0xFC9D|Lenovo (Singapore) Pte Ltd.|
|0xFC9E|Dell Computer Corporation|
|0xFC9F|Delta Development Team, Inc|
|0xFCA0|Apple Inc.|
|0xFCA1|PF SCHWEISSTECHNOLOGIE GMBH|
|0xFCA2|Meizu Technology Co., Ltd.|
|0xFCA3|Gunnebo Aktiebolag|
|0xFCA4|HP Inc.|
|0xFCA5|HAYWARD INDUSTRIES, INC.|
|0xFCA6|Hubble Network Inc.|
|0xFCA7|Hubble Network Inc.|
|0xFCA8|Medtronic Inc.|
|0xFCA9|Medtronic Inc.|
|0xFCAA|Spintly, Inc.|
|0xFCAB|IRISS INC.|
|0xFCAC|IRISS INC.|
|0xFCAD|Beijing 99help Safety Technology Co., Ltd|
|0xFCAE|Imagine Marketing Limited|
|0xFCAF|AltoBeam Inc.|
|0xFCB0|Ford Motor Company|
|0xFCB1|Google LLC|
|0xFCB2|Apple Inc.|
|0xFCB3|SWEEN|
|0xFCB4|OMRON HEALTHCARE Co., Ltd.|
|0xFCB5|OMRON HEALTHCARE Co., Ltd.|
|0xFCB6|OMRON HEALTHCARE Co., Ltd.|
|0xFCB7|T-Mobile USA|
|0xFCB8|Ribbiot, INC.|



Bluetooth SIG Proprietary 

Page 119 of 442 

**Assigned Numbers** / Document 

**2026-06-23** 

|0xFCB9|Lumi United Technology Co., Ltd|
|---|---|
|0xFCBA|BlueID GmbH|
|0xFCBB|SharkNinja Operating LLC|
|0xFCBC|Drowsy Digital, Inc.|
|0xFCBD|Toshiba Corporation|
|0xFCBE|Musen Connect, Inc.|
|0xFCBF|ASSA ABLOY Opening Solutions Sweden AB|
|0xFCC0|Xiaomi Inc.|
|0xFCC1|TIMECODE SYSTEMS LIMITED|
|0xFCC2|Qualcomm Technologies, Inc.|
|0xFCC3|HP Inc.|
|0xFCC4|OMRON(DALIAN) CO,.LTD.|
|0xFCC5|OMRON(DALIAN) CO,.LTD.|
|0xFCC6|Wiliot LTD.|
|0xFCC7|PB INC.|
|0xFCC8|Allthenticate, Inc.|
|0xFCC9|SkyHawke Technologies|
|0xFCCA|Cosmed s.r.l.|
|0xFCCB|TOTO LTD.|
|0xFCCD|Marshall Group AB|
|0xFCCE|Luna Health, Inc.|
|0xFCCF|Google LLC|
|0xFCD0|Laerdal Medical AS|
|0xFCD1|Shenzhen Benwei Media Co.,Ltd.|
|0xFCD2|Allterco Robotics ltd|
|0xFCD3|Fisher & Paykel Healthcare|
|0xFCD4|OMRON HEALTHCARE|
|0xFCD5|Nortek Security & Control|
|0xFCD6|SWISSINNO SOLUTIONS AG|
|0xFCD7|PowerPal Pty Ltd|
|0xFCD8|Appex Factory S.L.|
|0xFCD9|Huso, INC|
|0xFCDA|Draeger|
|0xFCDB|aconno GmbH|
|0xFCDC|Amazon.com Services, LLC|
|0xFCDD|Mobilaris AB|



Bluetooth SIG Proprietary 

Page 120 of 442 

**Assigned Numbers** / Document 

**2026-06-23** 

|0xFCDE|ARCTOP, INC.|
|---|---|
|0xFCDF|NIO USA, Inc.|
|0xFCE0|Akciju sabiedriba ”SAF TEHNIKA”|
|0xFCE1|Sony Group Corporation|
|0xFCE2|Baracoda Daily Healthtech|
|0xFCE3|Smith & Nephew Medical Limited|
|0xFCE4|Samsara Networks, Inc|
|0xFCE5|Samsara Networks, Inc|
|0xFCE6|Guard RFID Solutions Inc.|
|0xFCE7|TKH Security B.V.|
|0xFCE8|ITT Industries|
|0xFCE9|MindRhythm, Inc.|
|0xFCEA|Chess Wise B.V.|
|0xFCEB|Avi-On|
|0xFCEC|Griffwerk GmbH|
|0xFCED|Workaround Gmbh|
|0xFCEE|Velentium, LLC|
|0xFCEF|Divesoft s.r.o.|
|0xFCF0|Security Enhancement Systems, LLC|
|0xFCF1|Google LLC|
|0xFCF2|Bitwards Oy|
|0xFCF3|Armatura LLC|
|0xFCF4|Allegion|
|0xFCF5|Trident Communication Technology, LLC|
|0xFCF6|The Linux Foundation|
|0xFCF7|Honor Device Co., Ltd.|
|0xFCF8|Honor Device Co., Ltd.|
|0xFCF9|Leupold & Stevens, Inc.|
|0xFCFA|Leupold & Stevens, Inc.|
|0xFCFB|Shenzhen Benwei Media Co., Ltd.|
|0xFCFC|Barrot Technology Co.,Ltd.|
|0xFCFD|Barrot Technology Co.,Ltd.|
|0xFCFE|Sonova Consumer Hearing GmbH|
|0xFCFF|701x|
|0xFD00|FUTEK Advanced Sensor Technology, Inc.|
|0xFD01|Sanvita Medical Corporation|



Bluetooth SIG Proprietary 

Page 121 of 442 

**Assigned Numbers** / Document 

**2026-06-23** 

|0xFD02|LEGO System A/S|
|---|---|
|0xFD03|Quuppa Oy|
|0xFD04|Shure Inc.|
|0xFD05|Qualcomm Technologies, Inc.|
|0xFD06|RACE-AI LLC|
|0xFD07|Swedlock AB|
|0xFD08|Bull Group Incorporated Company|
|0xFD09|Cousins and Sears LLC|
|0xFD0A|Luminostics, Inc.|
|0xFD0B|Luminostics, Inc.|
|0xFD0C|OSM HK Limited|
|0xFD0D|Blecon Ltd|
|0xFD0E|HerdDogg, Inc|
|0xFD0F|AEON MOTOR CO.,LTD.|
|0xFD10|AEON MOTOR CO.,LTD.|
|0xFD11|AEON MOTOR CO.,LTD.|
|0xFD12|AEON MOTOR CO.,LTD.|
|0xFD13|BRG Sports, Inc.|
|0xFD14|BRG Sports, Inc.|
|0xFD15|Panasonic Corporation|
|0xFD16|Sensitech, Inc.|
|0xFD17|LEGIC Identsystems AG|
|0xFD18|LEGIC Identsystems AG|
|0xFD19|Smith & Nephew Medical Limited|
|0xFD1A|CSIRO|
|0xFD1B|Helios Sports, Inc.|
|0xFD1C|Brady Worldwide Inc.|
|0xFD1D|Samsung Electronics Co., Ltd|
|0xFD1E|Plume Design Inc.|
|0xFD1F|3M|
|0xFD20|GN Hearing A/S|
|0xFD21|Huawei Technologies Co., Ltd.|
|0xFD22|Huawei Technologies Co., Ltd.|
|0xFD23|DOM Sicherheitstechnik GmbH & Co. KG|
|0xFD24|GD Midea Air-Conditioning Equipment Co., Ltd.|
|0xFD25|GD Midea Air-Conditioning Equipment Co., Ltd.|



Bluetooth SIG Proprietary 

Page 122 of 442 

**Assigned Numbers** / Document 

**2026-06-23** 

|0xFD26|Novo Nordisk A/S|
|---|---|
|0xFD27|Integrated Illumination Systems, Inc.|
|0xFD28|Julius Blum GmbH|
|0xFD29|Asahi Kasei Corporation|
|0xFD2A|Sony Corporation|
|0xFD2B|The Access Technologies|
|0xFD2C|The Access Technologies|
|0xFD2D|Xiaomi Inc.|
|0xFD2E|Bitstrata Systems Inc.|
|0xFD2F|Bitstrata Systems Inc.|
|0xFD30|Sesam Solutions BV|
|0xFD31|LG Electronics Inc.|
|0xFD32|Gemalto Holding BV|
|0xFD33|DashLogic, Inc.|
|0xFD34|Aerosens LLC.|
|0xFD35|Transsion Holdings Limited|
|0xFD36|Google LLC|
|0xFD37|TireCheck GmbH|
|0xFD38|Danfoss A/S|
|0xFD39|PREDIKTAS|
|0xFD3A|Verkada Inc.|
|0xFD3B|Verkada Inc.|
|0xFD3C|Redline Communications Inc.|
|0xFD3D|Woan Technology (Shenzhen) Co., Ltd.|
|0xFD3E|Pure Watercraft, inc.|
|0xFD3F|Cognosos, Inc|
|0xFD40|Beflex Inc.|
|0xFD41|Amazon Lab126|
|0xFD42|Globe (Jiangsu) Co.,Ltd|
|0xFD43|Apple Inc.|
|0xFD44|Apple Inc.|
|0xFD45|GB Solution co.,Ltd|
|0xFD46|Lemco IKE|
|0xFD47|Liberty Global Inc.|
|0xFD48|Geberit International AG|
|0xFD49|Panasonic Corporation|



Bluetooth SIG Proprietary 

Page 123 of 442 

**Assigned Numbers** / Document 

**2026-06-23** 

|0xFD4A|Sigma Elektro GmbH|
|---|---|
|0xFD4B|Samsung Electronics Co., Ltd.|
|0xFD4C|Adolf Wuerth GmbH & Co KG|
|0xFD4D|70mai Co.,Ltd.|
|0xFD4E|70mai Co.,Ltd.|
|0xFD4F|SONITOR TECHNOLOGIES AS|
|0xFD50|Hangzhou Tuya Information Technology Co., Ltd|
|0xFD51|UTC Fire and Security|
|0xFD52|UTC Fire and Security|
|0xFD53|PCI Private Limited|
|0xFD54|Qingdao Haier Technology Co., Ltd.|
|0xFD55|Braveheart Wireless, Inc.|
|0xFD56|Resmed Ltd|
|0xFD57|Volvo Car Corporation|
|0xFD58|Volvo Car Corporation|
|0xFD59|Samsung Electronics Co., Ltd.|
|0xFD5A|Samsung Electronics Co., Ltd.|
|0xFD5B|V2SOFT INC.|
|0xFD5C|React Mobile|
|0xFD5D|maxon motor ltd.|
|0xFD5E|Tapkey GmbH|
|0xFD5F|Meta Platforms Technologies, LLC|
|0xFD60|Sercomm Corporation|
|0xFD61|Arendi AG|
|0xFD62|Google LLC|
|0xFD63|Google LLC|
|0xFD64|INRIA|
|0xFD65|Razer Inc.|
|0xFD66|Zebra Technologies Corporation|
|0xFD67|Montblanc Simplo GmbH|
|0xFD68|Ubique Innovation AG|
|0xFD69|Samsung Electronics Co., Ltd|
|0xFD6A|Emerson|
|0xFD6B|rapitag GmbH|
|0xFD6C|Samsung Electronics Co., Ltd.|
|0xFD6D|Sigma Elektro GmbH|



Bluetooth SIG Proprietary 

Page 124 of 442 

**Assigned Numbers** / Document 

**2026-06-23** 

|0xFD6E|Polidea sp. z o.o.|
|---|---|
|0xFD6F|Apple, Inc.|
|0xFD70|GuangDong Oppo Mobile Telecommunications Corp., Ltd|
|0xFD71|GN Hearing A/S|
|0xFD72|Logitech International SA|
|0xFD73|BRControls Products BV|
|0xFD74|BRControls Products BV|
|0xFD75|Insulet Corporation|
|0xFD76|Insulet Corporation|
|0xFD77|Withings|
|0xFD78|Withings|
|0xFD79|Withings|
|0xFD7A|Withings|
|0xFD7B|WYZE LABS, INC.|
|0xFD7C|Toshiba Information Systems(Japan) Corporation|
|0xFD7D|Center for Advanced Research Wernher Von Braun|
|0xFD7E|Samsung Electronics Co., Ltd.|
|0xFD7F|Husqvarna AB|
|0xFD80|Phindex Technologies, Inc|
|0xFD81|CANDY HOUSE, Inc.|
|0xFD82|Sony Corporation|
|0xFD83|iNFORM Technology GmbH|
|0xFD84|Tile, Inc.|
|0xFD85|Husqvarna AB|
|0xFD86|Abbott|
|0xFD87|Google LLC|
|0xFD88|Urbanminded LTD|
|0xFD89|Urbanminded LTD|
|0xFD8A|Signify Netherlands B.V.|
|0xFD8B|Jigowatts Inc.|
|0xFD8C|Google LLC|
|0xFD8D|quip NYC Inc.|
|0xFD8E|Motorola Solutions|
|0xFD90|Guangzhou SuperSound Information Technology Co.,Ltd|
|0xFD91|Groove X, Inc.|
|0xFD92|Qualcomm Technologies International, Ltd. (QTIL)|



Bluetooth SIG Proprietary 

Page 125 of 442 

**Assigned Numbers** / Document 

**2026-06-23** 

|0xFD93|Bayerische Motoren Werke AG|
|---|---|
|0xFD94|Hewlett Packard Enterprise|
|0xFD95|Rigado|
|0xFD96|Google LLC|
|0xFD97|June Life, Inc.|
|0xFD98|Disney Worldwide Services, Inc.|
|0xFD99|ABB Oy|
|0xFD9A|Huawei Technologies Co., Ltd.|
|0xFD9B|Huawei Technologies Co., Ltd.|
|0xFD9C|Huawei Technologies Co., Ltd.|
|0xFD9D|Gastec Corporation|
|0xFD9E|The Coca-Cola Company|
|0xFD9F|VitalTech Affiliates LLC|
|0xFDA0|Secugen Corporation|
|0xFDA1|Groove X, Inc|
|0xFDA2|Groove X, Inc|
|0xFDA3|Inseego Corp.|
|0xFDA4|Inseego Corp.|
|0xFDA5|Neurostim OAB, Inc.|
|0xFDA6|WWZN Information Technology Company Limited|
|0xFDA7|WWZN Information Technology Company Limited|
|0xFDA8|PSA Peugeot Citroën|
|0xFDA9|Rhombus Systems, Inc.|
|0xFDAA|Xiaomi Inc.|
|0xFDAB|Xiaomi Inc.|
|0xFDAC|Tentacle Sync GmbH|
|0xFDAD|Houwa System Design, k.k.|
|0xFDAE|Houwa System Design, k.k.|
|0xFDAF|Wiliot LTD|
|0xFDB0|Oura Health Ltd|
|0xFDB1|Oura Health Ltd|
|0xFDB2|Portable Multimedia Ltd|
|0xFDB3|Audiodo AB|
|0xFDB4|HP Inc|
|0xFDB5|ECSG|
|0xFDB6|GWA Hygiene GmbH|



Bluetooth SIG Proprietary 

Page 126 of 442 

**Assigned Numbers** / Document 

**2026-06-23** 

|0xFDB7|LivaNova USA Inc.|
|---|---|
|0xFDB8|LivaNova USA Inc.|
|0xFDBB|Profoto|
|0xFDBC|Emerson|
|0xFDBD|Clover Network, Inc.|
|0xFDBE|California Things Inc.|
|0xFDBF|California Things Inc.|
|0xFDC0|Hunter Douglas|
|0xFDC1|Hunter Douglas|
|0xFDC2|Baidu Online Network Technology (Beijing) Co., Ltd|
|0xFDC3|Baidu Online Network Technology (Beijing) Co., Ltd|
|0xFDC4|Simavita (Aust) Pty Ltd|
|0xFDC5|Automatic Labs|
|0xFDC6|Eli Lilly and Company|
|0xFDC7|Eli Lilly and Company|
|0xFDC8|Hach – Danaher|
|0xFDC9|Busch-Jaeger Elektro GmbH|
|0xFDCA|Fortin Electronic Systems|
|0xFDCB|Meggitt SA|
|0xFDCC|Shoof Technologies|
|0xFDCD|Qingping Technology (Beijing) Co., Ltd.|
|0xFDCE|SENNHEISER electronic GmbH & Co. KG|
|0xFDCF|Nalu Medical, Inc|
|0xFDD0|Huawei Technologies Co., Ltd|
|0xFDD1|Huawei Technologies Co., Ltd|
|0xFDD2|Bose Corporation|
|0xFDD3|FUBA Automotive Electronics GmbH|
|0xFDD4|LX Solutions Pty Limited|
|0xFDD5|Brompton Bicycle Ltd|
|0xFDD6|Ministry of Supply|
|0xFDD7|Copeland Cold Chain LP|
|0xFDD8|Jiangsu Teranovo Tech Co., Ltd.|
|0xFDD9|Jiangsu Teranovo Tech Co., Ltd.|
|0xFDDA|MHCS|
|0xFDDB|Samsung Electronics Co., Ltd.|
|0xFDDC|4iiii Innovations Inc.|



Bluetooth SIG Proprietary 

Page 127 of 442 

**Assigned Numbers** / Document 

**2026-06-23** 

|0xFDDD|Arch Systems Inc|
|---|---|
|0xFDDE|Noodle Technology Inc.|
|0xFDDF|Harman International|
|0xFDE0|John Deere|
|0xFDE1|Fortin Electronic Systems|
|0xFDE2|Google LLC|
|0xFDE3|Abbott Diabetes Care|
|0xFDE4|JUUL Labs, Inc.|
|0xFDE5|SMK Corporation|
|0xFDE6|Intelletto Technologies Inc|
|0xFDE7|SECOM Co., LTD|
|0xFDE8|Robert Bosch GmbH|
|0xFDE9|Spacesaver Corporation|
|0xFDEA|SeeScan, Inc|
|0xFDEB|Syntronix Corporation|
|0xFDEC|Mannkind Corporation|
|0xFDED|Pole Star|
|0xFDEE|Huawei Technologies Co., Ltd.|
|0xFDEF|ART AND PROGRAM, INC.|
|0xFDF0|Google LLC|
|0xFDF1|LAMPLIGHT Co.,Ltd|
|0xFDF2|AMICCOM Electronics Corporation|
|0xFDF3|Amersports|
|0xFDF4|O. E. M. Controls, Inc.|
|0xFDF5|Milwaukee Electric Tools|
|0xFDF6|AIAIAI ApS|
|0xFDF7|HP Inc.|
|0xFDF8|Onvocal|
|0xFDF9|INIA|
|0xFDFA|Tandem Diabetes Care|
|0xFDFB|Tandem Diabetes Care|
|0xFDFC|Optrel AG|
|0xFDFD|RecursiveSoft Inc.|
|0xFDFE|ADHERIUM(NZ) LIMITED|
|0xFDFF|OSRAM GmbH|
|0xFE00|Amazon.com Services, Inc.|



Bluetooth SIG Proprietary 

Page 128 of 442 

**Assigned Numbers** / Document 

**2026-06-23** 

|0xFE01|Duracell U.S. Operations Inc.|
|---|---|
|0xFE02|Robert Bosch GmbH|
|0xFE03|Amazon.com Services, Inc.|
|0xFE04|Motorola Solutions, Inc.|
|0xFE05|CORE Transport Technologies NZ Limited|
|0xFE06|Qualcomm Technologies, Inc.|
|0xFE07|Sonos, Inc.|
|0xFE08|Microsoft|
|0xFE09|Pillsy, Inc.|
|0xFE0A|ruwido austria gmbh|
|0xFE0B|ruwido austria gmbh|
|0xFE0C|Procter & Gamble|
|0xFE0D|Procter & Gamble|
|0xFE0E|Setec Pty Ltd|
|0xFE0F|Signify Netherlands B.V. (formerly Philips Lighting B.V.)|
|0xFE10|LAPIS Technology Co., Ltd.|
|0xFE11|GMC-I Messtechnik GmbH|
|0xFE12|M-Way Solutions GmbH|
|0xFE13|Apple Inc.|
|0xFE14|Flextronics International USA Inc.|
|0xFE15|Amazon.com Services, Inc..|
|0xFE16|Footmarks, Inc.|
|0xFE17|Telit Wireless Solutions GmbH|
|0xFE18|Runtime, Inc.|
|0xFE19|Google LLC|
|0xFE1A|Tyto Life LLC|
|0xFE1B|Tyto Life LLC|
|0xFE1C|NetMedia, Inc.|
|0xFE1D|Illuminati Instrument Corporation|
|0xFE1E|LAMPLIGHT Co., Ltd.|
|0xFE1F|Garmin International, Inc.|
|0xFE20|Emerson|
|0xFE21|Bose Corporation|
|0xFE22|Zoll Medical Corporation|
|0xFE23|Zoll Medical Corporation|
|0xFE24|August Home Inc|



Bluetooth SIG Proprietary 

Page 129 of 442 

**Assigned Numbers** / Document 

**2026-06-23** 

|0xFE25|Apple, Inc.|
|---|---|
|0xFE26|Google LLC|
|0xFE27|Google LLC|
|0xFE28|Ayla Networks|
|0xFE29|Gibson Innovations|
|0xFE2A|DaisyWorks, Inc.|
|0xFE2B|ITT Industries|
|0xFE2C|Google LLC|
|0xFE2D|LAMPLIGHT Co., Ltd.|
|0xFE2E|ERi,Inc.|
|0xFE2F|CRESCO Wireless, Inc|
|0xFE30|Volkswagen AG|
|0xFE31|Volkswagen AG|
|0xFE32|Pro-Mark, Inc.|
|0xFE33|CHIPOLO d.o.o.|
|0xFE34|SmallLoop LLC|
|0xFE35|HUAWEI Technologies Co., Ltd|
|0xFE36|HUAWEI Technologies Co., Ltd|
|0xFE39|TTS Tooltechnic Systems AG & Co. KG|
|0xFE3A|TTS Tooltechnic Systems AG & Co. KG|
|0xFE3B|Dolby Laboratories|
|0xFE3C|alibaba|
|0xFE3D|BD Medical|
|0xFE3E|BD Medical|
|0xFE3F|Friday Labs Limited|
|0xFE40|Inugo Systems Limited|
|0xFE41|Inugo Systems Limited|
|0xFE42|Nets A/S|
|0xFE43|Andreas Stihl AG & Co. KG|
|0xFE44|SK Telecom|
|0xFE45|Snapchat Inc|
|0xFE46|B&O Play A/S|
|0xFE47|General Motors|
|0xFE48|General Motors|
|0xFE49|SenionLab AB|
|0xFE4A|OMRON HEALTHCARE Co., Ltd.|



Bluetooth SIG Proprietary 

Page 130 of 442 

**Assigned Numbers** / Document 

**2026-06-23** 

|0xFE4B|Signify Netherlands B.V. (formerly Philips Lighting B.V.)|
|---|---|
|0xFE4C|Volkswagen AG|
|0xFE4D|Casambi Technologies Oy|
|0xFE4E|NTT docomo|
|0xFE4F|Molekule, Inc.|
|0xFE50|Google LLC|
|0xFE51|SRAM|
|0xFE52|SetPoint Medical|
|0xFE53|3M|
|0xFE54|Motiv, Inc.|
|0xFE55|Google LLC|
|0xFE56|Google LLC|
|0xFE57|Dotted Labs|
|0xFE58|Nordic Semiconductor ASA|
|0xFE59|Nordic Semiconductor ASA|
|0xFE5A|Cronologics Corporation|
|0xFE5B|GT-tronics HK Ltd|
|0xFE5C|million hunters GmbH|
|0xFE5D|Grundfos A/S|
|0xFE5E|Plastc Corporation|
|0xFE5F|Eyefi, Inc.|
|0xFE60|Lierda Science & Technology Group Co., Ltd.|
|0xFE61|Logitech International SA|
|0xFE62|Indagem Tech LLC|
|0xFE63|Connected Yard, Inc.|
|0xFE64|Siemens AG|
|0xFE65|CHIPOLO d.o.o.|
|0xFE66|Intel Corporation|
|0xFE67|Lab Sensor Solutions|
|0xFE68|Capsle Technologies Inc.|
|0xFE69|Capsle Technologies Inc.|
|0xFE6A|Kontakt Micro-Location Sp. z o.o.|
|0xFE6B|TASER International, Inc.|
|0xFE6C|TASER International, Inc.|
|0xFE6D|The University of Tokyo|
|0xFE6E|The University of Tokyo|



Bluetooth SIG Proprietary 

Page 131 of 442 

**Assigned Numbers** / Document 

**2026-06-23** 

|0xFE6F|LINE Corporation|
|---|---|
|0xFE70|Beijing Jingdong Century Trading Co., Ltd.|
|0xFE71|Plume Design Inc|
|0xFE72|Abbott (formerly St. Jude Medical, Inc.)|
|0xFE73|Abbott (formerly St. Jude Medical, Inc.)|
|0xFE74|unwire|
|0xFE75|TangoMe|
|0xFE76|TangoMe|
|0xFE77|Hewlett-Packard Company|
|0xFE78|Hewlett-Packard Company|
|0xFE79|Zebra Technologies|
|0xFE7A|Bragi GmbH|
|0xFE7B|Orion Labs, Inc.|
|0xFE7C|Telit Wireless Solutions (Formerly Stollmann E+V GmbH)|
|0xFE7D|Aterica Health Inc.|
|0xFE7E|Awear Solutions Ltd|
|0xFE7F|Doppler Lab|
|0xFE80|Doppler Lab|
|0xFE81|Medtronic Inc.|
|0xFE82|Medtronic Inc.|
|0xFE83|Blue Bite|
|0xFE84|RF Digital Corp|
|0xFE85|RF Digital Corp|
|0xFE86|HUAWEI Technologies Co., Ltd|
|0xFE87|Qingdao Yeelink Information Technology Co., Ltd. (<br>青岛亿联客信息技术有限公司)|
|0xFE88|SALTO SYSTEMS S.L.|
|0xFE89|B&O Play A/S|
|0xFE8A|Apple, Inc.|
|0xFE8B|Apple, Inc.|
|0xFE8C|TRON Forum|
|0xFE8D|Interaxon Inc.|
|0xFE8E|ARM Ltd|
|0xFE8F|CSR|
|0xFE90|JUMA|
|0xFE91|Shanghai Imilab Technology Co.,Ltd|



Bluetooth SIG Proprietary 

Page 132 of 442 

**Assigned Numbers** / Document 

**2026-06-23** 

|0xFE92|Jarden Safety & Security|
|---|---|
|0xFE93|OttoQ In|
|0xFE94|OttoQ In|
|0xFE95|Xiaomi Inc.|
|0xFE96|Tesla Motors Inc.|
|0xFE97|Tesla Motors Inc.|
|0xFE98|Currant Inc|
|0xFE99|Currant Inc|
|0xFE9A|Estimote|
|0xFE9B|Samsara Networks, Inc|
|0xFE9C|GSI Laboratories, Inc.|
|0xFE9D|Mobiquity Networks Inc|
|0xFE9E|Renesas Design Netherlands B.V.|
|0xFE9F|Google LLC|
|0xFEA0|Google LLC|
|0xFEA3|ITT Industries|
|0xFEA4|Paxton Access Ltd|
|0xFEA5|GoPro, Inc.|
|0xFEA6|GoPro, Inc.|
|0xFEA7|UTC Fire and Security|
|0xFEA8|Savant Systems LLC|
|0xFEA9|Savant Systems LLC|
|0xFEAA|Google LLC|
|0xFEAB|Nokia|
|0xFEAC|Nokia|
|0xFEAD|Nokia|
|0xFEAE|Nokia|
|0xFEAF|Nest Labs Inc|
|0xFEB0|Nest Labs Inc|
|0xFEB1|Electronics Tomorrow Limited|
|0xFEB2|Microsoft Corporation|
|0xFEB3|Taobao|
|0xFEB4|WiSilica Inc.|
|0xFEB5|WiSilica Inc.|
|0xFEB6|Vencer Co., Ltd|
|0xFEB7|Meta Platforms, Inc.|



Bluetooth SIG Proprietary 

Page 133 of 442 

**Assigned Numbers** / Document 

**2026-06-23** 

|0xFEB8|Meta Platforms, Inc.|
|---|---|
|0xFEB9|LG Electronics|
|0xFEBA|Tencent Holdings Limited|
|0xFEBB|adafruit industries|
|0xFEBC|Dexcom Inc|
|0xFEBD|Clover Network, Inc|
|0xFEBE|Bose Corporation|
|0xFEBF|Nod, Inc.|
|0xFEC0|KDDI Corporation|
|0xFEC1|KDDI Corporation|
|0xFEC2|Blue Spark Technologies, Inc.|
|0xFEC3|360fly, Inc.|
|0xFEC4|PLUS Location Systems|
|0xFEC5|Realtek Semiconductor Corp.|
|0xFEC6|Kocomojo, LLC|
|0xFEC7|Apple, Inc.|
|0xFEC8|Apple, Inc.|
|0xFEC9|Apple, Inc.|
|0xFECA|Apple, Inc.|
|0xFECB|Apple, Inc.|
|0xFECC|Apple, Inc.|
|0xFECD|Apple, Inc.|
|0xFECE|Apple, Inc.|
|0xFECF|Apple, Inc.|
|0xFED0|Apple, Inc.|
|0xFED1|Apple, Inc.|
|0xFED2|Apple, Inc.|
|0xFED3|Apple, Inc.|
|0xFED4|Apple, Inc.|
|0xFED5|Plantronics Inc.|
|0xFED6|Broadcom|
|0xFED7|Broadcom|
|0xFED8|Google LLC|
|0xFED9|Pebble Technology Corporation|
|0xFEDA|ISSC Technologies Corp.|
|0xFEDB|Perka, Inc.|



Bluetooth SIG Proprietary 

Page 134 of 442 

**Assigned Numbers** / Document 

**2026-06-23** 

|0xFEDC|Jawbone|
|---|---|
|0xFEDD|Jawbone|
|0xFEDE|Coin, Inc.|
|0xFEE0|Anhui Huami Information Technology Co., Ltd.|
|0xFEE1|Anhui Huami Information Technology Co., Ltd.|
|0xFEE2|Anki, Inc.|
|0xFEE3|Anki, Inc.|
|0xFEE4|Nordic Semiconductor ASA|
|0xFEE5|Nordic Semiconductor ASA|
|0xFEE6|Silvair, Inc.|
|0xFEE7|Tencent Holdings Limited.|
|0xFEE8|Quintic Corp.|
|0xFEE9|Quintic Corp.|
|0xFEEA|Swirl Networks, Inc.|
|0xFEEB|Swirl Networks, Inc.|
|0xFEEC|Tile, Inc.|
|0xFEED|Tile, Inc.|
|0xFEEE|Polar Electro Oy|
|0xFEEF|Polar Electro Oy|
|0xFEF0|Intel|
|0xFEF1|CSR|
|0xFEF2|CSR|
|0xFEF3|Google LLC|
|0xFEF4|Google LLC|
|0xFEF5|Dialog Semiconductor GmbH|
|0xFEF6|Wicentric, Inc.|
|0xFEF7|Aplix Corporation|
|0xFEF8|Aplix Corporation|
|0xFEF9|PayPal, Inc.|
|0xFEFA|PayPal, Inc.|
|0xFEFB|Telit Wireless Solutions (Formerly Stollmann E+V GmbH)|
|0xFEFC|Gimbal, Inc.|
|0xFEFD|Gimbal, Inc.|
|0xFEFE|GN Hearing A/S|
|0xFEFF|GN Netcom|



Bluetooth SIG Proprietary 

Page 135 of 442 

**Assigned Numbers** / Document 

**2026-06-23** 

#### **3.12 Mesh Profiles** 

Last Modified: 2025-06-30 

Filename: assigned_numbers/uuids/mesh_profile_uuids.yaml 

|**UUID**|**Mesh Profile Name**|
|---|---|
|0x1600|Ambient Light Sensor NLC Profile 1.0|
|0x1601|Basic Lightness Controller NLC Profile 1.0|
|0x1602|Basic Scene Selector NLC Profile 1.0|
|0x1603|Dimming Control NLC Profile 1.0|
|0x1604|Energy Monitor NLC Profile 1.0|
|0x1605|Occupancy Sensor NLC Profile 1.0|
|0x1606|HVAC Integration NLC Profile 1.0|



Bluetooth SIG Proprietary 

Page 136 of 442 

**Assigned Numbers** / Document 

**2026-06-23** 

**<u>4 Mesh</u>** 

The following section includes the assigned numbers used by the set of Mesh specifications [17] [18] [16] [14] [15] . 

Bluetooth SIG Proprietary 

Page 137 of 442 

**Assigned Numbers** / Document 

**2026-06-23** 

#### **4.1 Mesh Model Identifiers** 

The following section lists the model identifiers assigned to models in the set of Mesh specifications [17] [18] [16] [14] [15] . 

##### **4.1.1 Mesh Model Identifiers by Value** 

The table below lists the assigned model identifiers organized by model identifier value. Last Modified: 2024-02-05 

Filename: assigned_numbers/mesh/mesh_model_uuids.yaml 

|**Mesh Model**<br>**Identifier**|**Mesh Model Name**|
|---|---|
|0x0000|Configuration Server|
|0x0001|Configuration Client|
|0x0002|Health Server|
|0x0003|Health Client|
|0x0004|Remote Provisioning Server|
|0x0005|Remote Provisioning Client|
|0x0006|Directed Forwarding Configuration Server|
|0x0007|Directed Forwarding Configuration Client|
|0x0008|Bridge Configuration Server|
|0x0009|Bridge Configuration Client|
|0x000A|Mesh Private Beacon Server|
|0x000B|Mesh Private Beacon Client|
|0x000C|On-Demand Private Proxy Server|
|0x000D|On-Demand Private Proxy Client|
|0x000E|SAR Configuration Server|
|0x000F|SAR Configuration Client|
|0x0010|Opcodes Aggregator Server|
|0x0011|Opcodes Aggregator Client|
|0x0012|Large Composition Data Server|
|0x0013|Large Composition Data Client|
|0x0014|Solicitation PDU RPL Configuration Server|
|0x0015|Solicitation PDU RPL Configuration Client|
|0x1000|Generic OnOff Server|
|0x1001|Generic OnOff Client|
|0x1002|Generic Level Server|
|0x1003|Generic Level Client|



Bluetooth SIG Proprietary 

Page 138 of 442 

**Assigned Numbers** / Document 

**2026-06-23** 

|0x1004|Generic Default Transition Time Server|
|---|---|
|0x1005|Generic Default Transition Time Client|
|0x1006|Generic Power OnOff Server|
|0x1007|Generic Power OnOff Setup Server|
|0x1008|Generic Power OnOff Client|
|0x1009|Generic Power Level Server|
|0x100A|Generic Power Level Setup Server|
|0x100B|Generic Power Level Client|
|0x100C|Generic Battery Server|
|0x100D|Generic Battery Client|
|0x100E|Generic Location Server|
|0x100F|Generic Location Setup Server|
|0x1010|Generic Location Client|
|0x1011|Generic Admin Property Server|
|0x1012|Generic Manufacturer Property Server|
|0x1013|Generic User Property Server|
|0x1014|Generic Client Property Server|
|0x1015|Generic Property Client|
|0x1100|Sensor Server|
|0x1101|Sensor Setup Server|
|0x1102|Sensor Client|
|0x1200|Time Server|
|0x1201|Time Setup Server|
|0x1202|Time Client|
|0x1203|Scene Server|
|0x1204|Scene Setup Server|
|0x1205|Scene Client|
|0x1206|Scheduler Server|
|0x1207|Scheduler Setup Server|
|0x1208|Scheduler Client|
|0x1300|Light Lightness Server|
|0x1301|Light Lightness Setup Server|
|0x1302|Light Lightness Client|
|0x1303|Light CTL Server|
|0x1304|Light CTL Setup Server|
|0x1305|Light CTL Client|



Bluetooth SIG Proprietary 

Page 139 of 442 

**Assigned Numbers** / Document 

**2026-06-23** 

|0x1306|Light CTL Temperature Server|
|---|---|
|0x1307|Light HSL Server|
|0x1308|Light HSL Setup Server|
|0x1309|Light HSL Client|
|0x130A|Light HSL Hue Server|
|0x130B|Light HSL Saturation Server|
|0x130C|Light xyL Server|
|0x130D|Light xyL Setup Server|
|0x130E|Light xyL Client|
|0x130F|Light LC Server|
|0x1310|Light LC Setup Server|
|0x1311|Light LC Client|
|0x1312|IEC 62386-104 Model|
|0x1400|BLOB Transfer Server|
|0x1401|BLOB Transfer Client|
|0x1402|Firmware Update Server|
|0x1403|Firmware Update Client|
|0x1404|Firmware Distribution Server|
|0x1405|Firmware Distribution Client|



##### **4.1.2 Mesh Model Identifiers by Name** 

The table below lists the assigned model identifiers organized by model name. 

###### Last Modified: 2024-02-05 

Filename: assigned_numbers/mesh/mesh_model_uuids.yaml 

|**Mesh Model Name**|**Mesh Model**<br>**Identifier**|
|---|---|
|BLOB Transfer Client|0x1401|
|BLOB Transfer Server|0x1400|
|Bridge Configuration Client|0x0009|
|Bridge Configuration Server|0x0008|
|Configuration Client|0x0001|
|Configuration Server|0x0000|
|Directed Forwarding Configuration Client|0x0007|
|Directed Forwarding Configuration Server|0x0006|
|Firmware Distribution Client|0x1405|



Bluetooth SIG Proprietary 

Page 140 of 442 

**Assigned Numbers** / Document 

**2026-06-23** 

|Firmware Distribution Server|0x1404|
|---|---|
|Firmware Update Client|0x1403|
|Firmware Update Server|0x1402|
|Generic Admin Property Server|0x1011|
|Generic Battery Client|0x100D|
|Generic Battery Server|0x100C|
|Generic Client Property Server|0x1014|
|Generic Default Transition Time Client|0x1005|
|Generic Default Transition Time Server|0x1004|
|Generic Level Client|0x1003|
|Generic Level Server|0x1002|
|Generic Location Client|0x1010|
|Generic Location Server|0x100E|
|Generic Location Setup Server|0x100F|
|Generic Manufacturer Property Server|0x1012|
|Generic OnOff Client|0x1001|
|Generic OnOff Server|0x1000|
|Generic Power Level Client|0x100B|
|Generic Power Level Server|0x1009|
|Generic Power Level Setup Server|0x100A|
|Generic Power OnOff Client|0x1008|
|Generic Power OnOff Server|0x1006|
|Generic Power OnOff Setup Server|0x1007|
|Generic Property Client|0x1015|
|Generic User Property Server|0x1013|
|Health Client|0x0003|
|Health Server|0x0002|
|IEC 62386-104 Model|0x1312|
|Large Composition Data Client|0x0013|
|Large Composition Data Server|0x0012|
|Light CTL Client|0x1305|
|Light CTL Server|0x1303|
|Light CTL Setup Server|0x1304|
|Light CTL Temperature Server|0x1306|
|Light HSL Client|0x1309|
|Light HSL Hue Server<br>Bluetooth SIG Proprietary|0x130A<br>P|



Page 141 of 442 

**Assigned Numbers** / Document 

**2026-06-23** 

|Light HSL Saturation Server|0x130B|
|---|---|
|Light HSL Server|0x1307|
|Light HSL Setup Server|0x1308|
|Light LC Client|0x1311|
|Light LC Server|0x130F|
|Light LC Setup Server|0x1310|
|Light Lightness Client|0x1302|
|Light Lightness Server|0x1300|
|Light Lightness Setup Server|0x1301|
|Light xyL Client|0x130E|
|Light xyL Server|0x130C|
|Light xyL Setup Server|0x130D|
|Mesh Private Beacon Client|0x000B|
|Mesh Private Beacon Server|0x000A|
|On-Demand Private Proxy Client|0x000D|
|On-Demand Private Proxy Server|0x000C|
|Opcodes Aggregator Client|0x0011|
|Opcodes Aggregator Server|0x0010|
|Remote Provisioning Client|0x0005|
|Remote Provisioning Server|0x0004|
|SAR Configuration Client|0x000F|
|SAR Configuration Server|0x000E|
|Scene Client|0x1205|
|Scene Server|0x1203|
|Scene Setup Server|0x1204|
|Scheduler Client|0x1208|
|Scheduler Server|0x1206|
|Scheduler Setup Server|0x1207|
|Sensor Client|0x1102|
|Sensor Server|0x1100|
|Sensor Setup Server|0x1101|
|Solicitation PDU RPL Configuration Client|0x0015|
|Solicitation PDU RPL Configuration Server|0x0014|
|Time Client|0x1202|
|Time Server|0x1200|
|Time Setup Server<br>Bluetooth SIG Proprietary|0x1201<br>P|



Page 142 of 442 

**Assigned Numbers** / Document 

**2026-06-23** 

#### **4.2 Mesh Model Message Opcodes** 

The following section lists the assigned values for model message opcodes in the set of Mesh specifications [17] [18] [16] [14] [15] . 

##### **4.2.1 Mesh Model Message Opcodes by Value** 

The table below lists the assigned numbers for model message opcodes organized by message opcode value. 

###### Last Modified: 2024-02-05 

Filename: assigned_numbers/mesh/mmdl_opcodes.yaml 

|**Message Opcode**|**Message Name**|
|---|---|
|0x00|Config AppKey Add|
|0x01|Config AppKey Update|
|0x02|Config Composition Data Status|
|0x03|Config Model Publication Set|
|0x04|Health Current Status|
|0x05|Health Fault Status|
|0x06|Config Heartbeat Publication Status|
|0x40|Generic Location Global Status|
|0x41|Generic Location Global Set|
|0x42|Generic Location Global Set Unacknowledged|
|0x43|Generic Manufacturer Properties Status|
|0x44|Generic Manufacturer Property Set|
|0x45|Generic Manufacturer Property Set Unacknowledged|
|0x46|Generic Manufacturer Property Status|
|0x47|Generic Admin Properties Status|
|0x48|Generic Admin Property Set|
|0x49|Generic Admin Property Set Unacknowledged|
|0x4A|Generic Admin Property Status|
|0x4B|Generic User Properties Status|
|0x4C|Generic User Property Set|
|0x4D|Generic User Property Set Unacknowledged|
|0x4E|Generic User Property Status|
|0x4F|Generic Client Properties Get|
|0x50|Generic Client Properties Status|
|0x51|Sensor Descriptor Status|
|0x52|Sensor Status|



Bluetooth SIG Proprietary 

Page 143 of 442 

**Assigned Numbers** / Document 

**2026-06-23** 

|0x53|Sensor Column Status|
|---|---|
|0x54|Sensor Series Status|
|0x55|Sensor Cadence Set|
|0x56|Sensor Cadence Set Unacknowledged|
|0x57|Sensor Cadence Status|
|0x58|Sensor Settings Status|
|0x59|Sensor Setting Set|
|0x5A|Sensor Setting Set Unacknowledged|
|0x5B|Sensor Setting Status|
|0x5C|Time Set|
|0x5D|Time Status|
|0x5E|Scene Status|
|0x5F|Scheduler Action Status|
|0x60|Scheduler Action Set|
|0x61|Scheduler Action Set Unacknowledged|
|0x62|Light LC Property Set|
|0x63|Light LC Property Set Unacknowledged|
|0x64|Light LC Property Status|
|0x65|IEC 62386-104 Frame|
|0x66|BLOB Chunk Transfer|
|0x67|BLOB Block Status|
|0x68|BLOB Partial Block Report|
|0x80 0x00|Config AppKey Delete|
|0x80 0x01|Config AppKey Get|
|0x80 0x02|Config AppKey List|
|0x80 0x03|Config AppKey Status|
|0x80 0x04|Health Attention Get|
|0x80 0x05|Health Attention Set|
|0x80 0x06|Health Attention Set Unacknowledged|
|0x80 0x07|Health Attention Status|
|0x80 0x08|Config Composition Data Get|
|0x80 0x09|Config Beacon Get|
|0x80 0x0A|Config Beacon Set|
|0x80 0x0B|Config Beacon Status|
|0x80 0x0C|Config Default TTL Get|
|0x80 0x0D<br>Bluetooth SIG|Config Default TTL Set<br>Proprietary|



Page 144 of 442 

**Assigned Numbers** / Document 

**2026-06-23** 

|0x80 0x0E|Config Default TTL Status|
|---|---|
|0x80 0x0F|Config Friend Get|
|0x80 0x10|Config Friend Set|
|0x80 0x11|Config Friend Status|
|0x80 0x12|Config GATT Proxy Get|
|0x80 0x13|Config GATT Proxy Set|
|0x80 0x14|Config GATT Proxy Status|
|0x80 0x15|Config Key Refresh Phase Get|
|0x80 0x16|Config Key Refresh Phase Set|
|0x80 0x17|Config Key Refresh Phase Status|
|0x80 0x18|Config Model Publication Get|
|0x80 0x19|Config Model Publication Status|
|0x80 0x1A|Config Model Publication Virtual Address Set|
|0x80 0x1B|Config Model Subscription Add|
|0x80 0x1C|Config Model Subscription Delete|
|0x80 0x1D|Config Model Subscription Delete All|
|0x80 0x1E|Config Model Subscription Overwrite|
|0x80 0x1F|Config Model Subscription Status|
|0x80 0x20|Config Model Subscription Virtual Address Add|
|0x80 0x21|Config Model Subscription Virtual Address Delete|
|0x80 0x22|Config Model Subscription Virtual Address Overwrite|
|0x80 0x23|Config Network Transmit Get|
|0x80 0x24|Config Network Transmit Set|
|0x80 0x25|Config Network Transmit Status|
|0x80 0x26|Config Relay Get|
|0x80 0x27|Config Relay Set|
|0x80 0x28|Config Relay Status|
|0x80 0x29|Config SIG Model Subscription Get|
|0x80 0x2A|Config SIG Model Subscription List|
|0x80 0x2B|Config Vendor Model Subscription Get|
|0x80 0x2C|Config Vendor Model Subscription List|
|0x80 0x2D|Config Low Power Node PollTimeout Get|
|0x80 0x2E|Config Low Power Node PollTimeout Status|
|0x80 0x2F|Health Fault Clear|
|0x80 0x30|Health Fault Clear Unacknowledged|
|0x80 0x31|Health Fault Get|



Bluetooth SIG Proprietary 

Page 145 of 442 

**Assigned Numbers** / Document 

**2026-06-23** 

|0x80 0x32|Health Fault Test|
|---|---|
|0x80 0x33|Health Fault Test Unacknowledged|
|0x80 0x34|Health Period Get|
|0x80 0x35|Health Period Set|
|0x80 0x36|Health Period Set Unacknowledged|
|0x80 0x37|Health Period Status|
|0x80 0x38|Config Heartbeat Publication Get|
|0x80 0x39|Config Heartbeat Publication Set|
|0x80 0x3A|Config Heartbeat Subscription Get|
|0x80 0x3B|Config Heartbeat Subscription Set|
|0x80 0x3C|Config Heartbeat Subscription Status|
|0x80 0x3D|Config Model App Bind|
|0x80 0x3E|Config Model App Status|
|0x80 0x3F|Config Model App Unbind|
|0x80 0x40|Config NetKey Add|
|0x80 0x41|Config NetKey Delete|
|0x80 0x42|Config NetKey Get|
|0x80 0x43|Config NetKey List|
|0x80 0x44|Config NetKey Status|
|0x80 0x45|Config NetKey Update|
|0x80 0x46|Config Node Identity Get|
|0x80 0x47|Config Node Identity Set|
|0x80 0x48|Config Node Identity Status|
|0x80 0x49|Config Node Reset|
|0x80 0x4A|Config Node Reset Status|
|0x80 0x4B|Config SIG Model App Get|
|0x80 0x4C|Config SIG Model App List|
|0x80 0x4D|Config Vendor Model App Get|
|0x80 0x4E|Config Vendor Model App List|
|0x80 0x4F|Remote Provisioning Scan Capabilities Get|
|0x80 0x50|Remote Provisioning Scan Capabilities Status|
|0x80 0x51|Remote Provisioning Scan Get|
|0x80 0x52|Remote Provisioning Scan Start|
|0x80 0x53|Remote Provisioning Scan Stop|
|0x80 0x54|Remote Provisioning Scan Status|
|0x80 0x55<br>Bluetooth SIG|Remote Provisioning Scan Report<br>Proprietary|



Page 146 of 442 

**Assigned Numbers** / Document 

**2026-06-23** 

|0x80 0x56|Remote Provisioning Extended Scan Start|
|---|---|
|0x80 0x57|Remote Provisioning Extended Scan Report|
|0x80 0x58|Remote Provisioning Link Get|
|0x80 0x59|Remote Provisioning Link Open|
|0x80 0x5A|Remote Provisioning Link Close|
|0x80 0x5B|Remote Provisioning Link Status|
|0x80 0x5C|Remote Provisioning Link Report|
|0x80 0x5D|Remote Provisioning PDU Send|
|0x80 0x5E|Remote Provisioning PDU Outbound Report|
|0x80 0x5F|Remote Provisioning PDU Report|
|0x80 0x60|PRIVATE_BEACON_GET|
|0x80 0x61|PRIVATE_BEACON_SET|
|0x80 0x62|PRIVATE_BEACON_STATUS|
|0x80 0x63|PRIVATE_GATT_PROXY_GET|
|0x80 0x64|PRIVATE_GATT_PROXY_SET|
|0x80 0x65|PRIVATE_GATT_PROXY_STATUS|
|0x80 0x66|PRIVATE_NODE_IDENTITY_GET|
|0x80 0x67|PRIVATE_NODE_IDENTITY_SET|
|0x80 0x68|PRIVATE_NODE_IDENTITY_STATUS|
|0x80 0x69|ON_DEMAND_PRIVATE_PROXY_GET|
|0x80 0x6A|ON_DEMAND_PRIVATE_PROXY_SET|
|0x80 0x6B|ON_DEMAND_PRIVATE_PROXY_STATUS|
|0x80 0x6C|SAR_TRANSMITTER_GET|
|0x80 0x6D|SAR_TRANSMITTER_SET|
|0x80 0x6E|SAR_TRANSMITTER_STATUS|
|0x80 0x6F|SAR_RECEIVER_GET|
|0x80 0x70|SAR_RECEIVER_SET|
|0x80 0x71|SAR_RECEIVER_STATUS|
|0x80 0x72|OPCODES_AGGREGATOR_SEQUENCE|
|0x80 0x73|OPCODES_AGGREGATOR_STATUS|
|0x80 0x74|LARGE_COMPOSITION_DATA_GET|
|0x80 0x75|LARGE_COMPOSITION_DATA_STATUS|
|0x80 0x76|MODELS_METADATA_GET|
|0x80 0x77|MODELS_METADATA_STATUS|
|0x80 0x78|SOLICITATION_PDU_RPL_ITEM_CLEAR|
|0x80 0x79<br>Bluetooth SIG|SOLICITATION_PDU_RPL_ITEM_CLEAR_UNACKNOWLEDGED<br>Proprietary|



Page 147 of 442 

**Assigned Numbers** / Document 

**2026-06-23** 

|0x80 0x7A|SOLICITATION_PDU_RPL_ITEM_STATUS|
|---|---|
|0x80 0x7B|DIRECTED_CONTROL_GET|
|0x80 0x7C|DIRECTED_CONTROL_SET|
|0x80 0x7D|DIRECTED_CONTROL_STATUS|
|0x80 0x7E|PATH_METRIC_GET|
|0x80 0x7F|PATH_METRIC_SET|
|0x80 0x80|PATH_METRIC_STATUS|
|0x80 0x81|DISCOVERY_TABLE_CAPABILITIES_GET|
|0x80 0x82|DISCOVERY_TABLE_CAPABILITIES_SET|
|0x80 0x83|DISCOVERY_TABLE_CAPABILITIES_STATUS|
|0x80 0x84|FORWARDING_TABLE_ADD|
|0x80 0x85|FORWARDING_TABLE_DELETE|
|0x80 0x86|FORWARDING_TABLE_STATUS|
|0x80 0x87|FORWARDING_TABLE_DEPENDENTS_ADD|
|0x80 0x88|FORWARDING_TABLE_DEPENDENTS_DELETE|
|0x80 0x89|FORWARDING_TABLE_DEPENDENTS_STATUS|
|0x80 0x8A|FORWARDING_TABLE_DEPENDENTS_GET|
|0x80 0x8B|FORWARDING_TABLE_DEPENDENTS_GET_STATUS|
|0x80 0x8C|FORWARDING_TABLE_ENTRIES_COUNT_GET|
|0x80 0x8D|FORWARDING_TABLE_ENTRIES_COUNT_STATUS|
|0x80 0x8E|FORWARDING_TABLE_ENTRIES_GET|
|0x80 0x8F|FORWARDING_TABLE_ENTRIES_STATUS|
|0x80 0x90|WANTED_LANES_GET|
|0x80 0x91|WANTED_LANES_SET|
|0x80 0x92|WANTED_LANES_STATUS|
|0x80 0x93|TWO_WAY_PATH_GET|
|0x80 0x94|TWO_WAY_PATH_SET|
|0x80 0x95|TWO_WAY_PATH_STATUS|
|0x80 0x96|PATH_ECHO_INTERVAL_GET|
|0x80 0x97|PATH_ECHO_INTERVAL_SET|
|0x80 0x98|PATH_ECHO_INTERVAL_STATUS|
|0x80 0x99|DIRECTED_NETWORK_TRANSMIT_GET|
|0x80 0x9A|DIRECTED_NETWORK_TRANSMIT_SET|
|0x80 0x9B|DIRECTED_NETWORK_TRANSMIT_STATUS|
|0x80 0x9C|DIRECTED_RELAY_RETRANSMIT_GET|
|0x80 0x9D<br>Bluetooth SIG|DIRECTED_RELAY_RETRANSMIT_SET<br>Proprietary|



Page 148 of 442 

**Assigned Numbers** / Document 

**2026-06-23** 

|0x80 0x9E|DIRECTED_RELAY_RETRANSMIT_STATUS|
|---|---|
|0x80 0x9F|RSSI_THRESHOLD_GET|
|0x80 0xA0|RSSI_THRESHOLD_SET|
|0x80 0xA1|RSSI_THRESHOLD_STATUS|
|0x80 0xA2|DIRECTED_PATHS_GET|
|0x80 0xA3|DIRECTED_PATHS_STATUS|
|0x80 0xA4|DIRECTED_PUBLISH_POLICY_GET|
|0x80 0xA5|DIRECTED_PUBLISH_POLICY_SET|
|0x80 0xA6|DIRECTED_PUBLISH_POLICY_STATUS|
|0x80 0xA7|PATH_DISCOVERY_TIMING_CONTROL_GET|
|0x80 0xA8|PATH_DISCOVERY_TIMING_CONTROL_SET|
|0x80 0xA9|PATH_DISCOVERY_TIMING_CONTROL_STATUS|
|0x80 0xAA|this opcode is not used|
|0x80 0xAB|DIRECTED_CONTROL_NETWORK_TRANSMIT_GET|
|0x80 0xAC|DIRECTED_CONTROL_NETWORK_TRANSMIT_SET|
|0x80 0xAD|DIRECTED_CONTROL_NETWORK_TRANSMIT_STATUS|
|0x80 0xAE|DIRECTED_CONTROL_RELAY_RETRANSMIT_GET|
|0x80 0xAF|DIRECTED_CONTROL_RELAY_RETRANSMIT_SET|
|0x80 0xB0|DIRECTED_CONTROL_RELAY_RETRANSMIT_STATUS|
|0x80 0xB1|SUBNET_BRIDGE_GET|
|0x80 0xB2|SUBNET_BRIDGE_SET|
|0x80 0xB3|SUBNET_BRIDGE_STATUS|
|0x80 0xB4|BRIDGING_TABLE_ADD|
|0x80 0xB5|BRIDGING_TABLE_REMOVE|
|0x80 0xB6|BRIDGING_TABLE_STATUS|
|0x80 0xB7|BRIDGED_SUBNETS_GET|
|0x80 0xB8|BRIDGED_SUBNETS_LIST|
|0x80 0xB9|BRIDGING_TABLE_GET|
|0x80 0xBA|BRIDGING_TABLE_LIST|
|0x80 0xBB|BRIDGING_TABLE_SIZE_GET|
|0x80 0xBC|BRIDGING_TABLE_SIZE_STATUS|
|0x82 0x01|Generic OnOff Get|
|0x82 0x02|Generic OnOff Set|
|0x82 0x03|Generic OnOff Set Unacknowledged|
|0x82 0x04|Generic OnOff Status|
|0x82 0x05|Generic Level Get|



Bluetooth SIG Proprietary 

Page 149 of 442 

**Assigned Numbers** / Document 

**2026-06-23** 

|0x82 0x06|Generic Level Set|
|---|---|
|0x82 0x07|Generic Level Set Unacknowledged|
|0x82 0x08|Generic Level Status|
|0x82 0x09|Generic Delta Set|
|0x82 0x0A|Generic Delta Set Unacknowledged|
|0x82 0x0B|Generic Move Set|
|0x82 0x0C|Generic Move Set Unacknowledged|
|0x82 0x0D|Generic Default Transition Time Get|
|0x82 0x0E|Generic Default Transition Time Set|
|0x82 0x0F|Generic Default Transition Time Set Unacknowledged|
|0x82 0x10|Generic Default Transition Time Status|
|0x82 0x11|Generic OnPowerUp Get|
|0x82 0x12|Generic OnPowerUp Status|
|0x82 0x13|Generic OnPowerUp Set|
|0x82 0x14|Generic OnPowerUp Set Unacknowledged|
|0x82 0x15|Generic Power Level Get|
|0x82 0x16|Generic Power Level Set|
|0x82 0x17|Generic Power Level Set Unacknowledged|
|0x82 0x18|Generic Power Level Status|
|0x82 0x19|Generic Power Last Get|
|0x82 0x1A|Generic Power Last Status|
|0x82 0x1B|Generic Power Default Get|
|0x82 0x1C|Generic Power Default Status|
|0x82 0x1D|Generic Power Range Get|
|0x82 0x1E|Generic Power Range Status|
|0x82 0x1F|Generic Power Default Set|
|0x82 0x20|Generic Power Default Set Unacknowledged|
|0x82 0x21|Generic Power Range Set|
|0x82 0x22|Generic Power Range Set Unacknowledged|
|0x82 0x23|Generic Battery Get|
|0x82 0x24|Generic Battery Status|
|0x82 0x25|Generic Location Global Get|
|0x82 0x26|Generic Location Local Get|
|0x82 0x27|Generic Location Local Status|
|0x82 0x28|Generic Location Local Set|
|0x82 0x29<br>Bluetooth SIG|Generic Location Local Set Unacknowledged<br>Proprietary|



Page 150 of 442 

**Assigned Numbers** / Document 

**2026-06-23** 

|0x82|0x2A<br>Generic Manufacturer Properties Get|
|---|---|
|0x82|0x2B<br>Generic Manufacturer Property Get|
|0x82|0x2C<br>Generic Admin Properties Get|
|0x82|0x2D<br>Generic Admin Property Get|
|0x82|0x2E<br>Generic User Properties Get|
|0x82|0x2F<br>Generic User Property Get|
|0x82|0x30<br>Sensor Descriptor Get|
|0x82|0x31<br>Sensor Get|
|0x82|0x32<br>Sensor Column Get|
|0x82|0x33<br>Sensor Series Get|
|0x82|0x34<br>Sensor Cadence Get|
|0x82|0x35<br>Sensor Settings Get|
|0x82|0x36<br>Sensor Setting Get|
|0x82|0x37<br>Time Get|
|0x82|0x38<br>Time Role Get|
|0x82|0x39<br>Time Role Set|
|0x82|0x3A<br>Time Role Status|
|0x82|0x3B<br>Time Zone Get|
|0x82|0x3C<br>Time Zone Set|
|0x82|0x3D<br>Time Zone Status|
|0x82|0x3E<br>TAI-UTC Delta Get|
|0x82|0x3F<br>TAI-UTC Delta Set|
|0x82|0x40<br>TAI-UTC Delta Status|
|0x82|0x41<br>Scene Get|
|0x82|0x42<br>Scene Recall|
|0x82|0x43<br>Scene Recall Unacknowledged|
|0x82|0x44<br>Scene Register Get|
|0x82|0x45<br>Scene Register Status|
|0x82|0x46<br>Scene Store|
|0x82|0x47<br>Scene Store Unacknowledged|
|0x82|0x48<br>Scheduler Action Get|
|0x82|0x49<br>Scheduler Get|
|0x82|0x4A<br>Scheduler Status|
|0x82|0x4B<br>Light Lightness Get|
|0x82|0x4C<br>Light Lightness Set|
|0x82 <br>Blu|0x4D<br>Light Lightness Set Unacknowledged<br>etooth SIG Proprietary|



Page 151 of 442 

**Assigned Numbers** / Document 

**2026-06-23** 

|0x82|0x4E<br>Light Lightness Status|
|---|---|
|0x82|0x4F<br>Light Lightness Linear Get|
|0x82|0x50<br>Light Lightness Linear Set|
|0x82|0x51<br>Light Lightness Linear Set Unacknowledged|
|0x82|0x52<br>Light Lightness Linear Status|
|0x82|0x53<br>Light Lightness Last Get|
|0x82|0x54<br>Light Lightness Last Status|
|0x82|0x55<br>Light Lightness Default Get|
|0x82|0x56<br>Light Lightness Default Status|
|0x82|0x57<br>Light Lightness Range Get|
|0x82|0x58<br>Light Lightness Range Status|
|0x82|0x59<br>Light Lightness Default Set|
|0x82|0x5A<br>Light Lightness Default Set Unacknowledged|
|0x82|0x5B<br>Light Lightness Range Set|
|0x82|0x5C<br>Light Lightness Range Set Unacknowledged|
|0x82|0x5D<br>Light CTL Get|
|0x82|0x5E<br>Light CTL Set|
|0x82|0x5F<br>Light CTL Set Unacknowledged|
|0x82|0x60<br>Light CTL Status|
|0x82|0x61<br>Light CTL Temperature Get|
|0x82|0x62<br>Light CTL Temperature Range Get|
|0x82|0x63<br>Light CTL Temperature Range Status|
|0x82|0x64<br>Light CTL Temperature Set|
|0x82|0x65<br>Light CTL Temperature Set Unacknowledged|
|0x82|0x66<br>Light CTL Temperature Status|
|0x82|0x67<br>Light CTL Default Get|
|0x82|0x68<br>Light CTL Default Status|
|0x82|0x69<br>Light CTL Default Set|
|0x82|0x6A<br>Light CTL Default Set Unacknowledged|
|0x82|0x6B<br>Light CTL Temperature Range Set|
|0x82|0x6C<br>Light CTL Temperature Range Set Unacknowledged|
|0x82|0x6D<br>Light HSL Get|
|0x82|0x6E<br>Light HSL Hue Get|
|0x82|0x6F<br>Light HSL Hue Set|
|0x82|0x70<br>Light HSL Hue Set Unacknowledged|
|0x82 <br>Blu|0x71<br>Light HSL Hue Status<br>etooth SIG Proprietary|



Page 152 of 442 

**Assigned Numbers** / Document 

**2026-06-23** 

|0x82|0x72<br>Light HSL Saturation Get|
|---|---|
|0x82|0x73<br>Light HSL Saturation Set|
|0x82|0x74<br>Light HSL Saturation Set Unacknowledged|
|0x82|0x75<br>Light HSL Saturation Status|
|0x82|0x76<br>Light HSL Set|
|0x82|0x77<br>Light HSL Set Unacknowledged|
|0x82|0x78<br>Light HSL Status|
|0x82|0x79<br>Light HSL Target Get|
|0x82|0x7A<br>Light HSL Target Status|
|0x82|0x7B<br>Light HSL Default Get|
|0x82|0x7C<br>Light HSL Default Status|
|0x82|0x7D<br>Light HSL Range Get|
|0x82|0x7E<br>Light HSL Range Status|
|0x82|0x7F<br>Light HSL Default Set|
|0x82|0x80<br>Light HSL Default Set Unacknowledged|
|0x82|0x81<br>Light HSL Range Set|
|0x82|0x82<br>Light HSL Range Set Unacknowledged|
|0x82|0x83<br>Light xyL Get|
|0x82|0x84<br>Light xyL Set|
|0x82|0x85<br>Light xyL Set Unacknowledged|
|0x82|0x86<br>Light xyL Status|
|0x82|0x87<br>Light xyL Target Get|
|0x82|0x88<br>Light xyL Target Status|
|0x82|0x89<br>Light xyL Default Get|
|0x82|0x8A<br>Light xyL Default Status|
|0x82|0x8B<br>Light xyL Range Get|
|0x82|0x8C<br>Light xyL Range Status|
|0x82|0x8D<br>Light xyL Default Set|
|0x82|0x8E<br>Light xyL Default Set Unacknowledged|
|0x82|0x8F<br>Light xyL Range Set|
|0x82|0x90<br>Light xyL Range Set Unacknowledged|
|0x82|0x91<br>Light LC Mode Get|
|0x82|0x92<br>Light LC Mode Set|
|0x82|0x93<br>Light LC Mode Set Unacknowledged|
|0x82|0x94<br>Light LC Mode Status|
|0x82 <br>Blu|0x95<br>Light LC OM Get<br>etooth SIG Proprietary|



Page 153 of 442 

**Assigned Numbers** / Document 

**2026-06-23** 

|0x82 0x96|Light LC OM Set|
|---|---|
|0x82 0x97|Light LC OM Set Unacknowledged|
|0x82 0x98|Light LC OM Status|
|0x82 0x99|Light LC Light OnOff Get|
|0x82 0x9A|Light LC Light OnOff Set|
|0x82 0x9B|Light LC Light OnOff Set Unacknowledged|
|0x82 0x9C|Light LC Light OnOff Status|
|0x82 0x9D|Light LC Property Get|
|0x82 0x9E|Scene Delete|
|0x82 0x9F|Scene Delete Unacknowledged|
|0x83 0x00|BLOB Transfer Get|
|0x83 0x01|BLOB Transfer Start|
|0x83 0x02|BLOB Transfer Cancel|
|0x83 0x03|BLOB Transfer Status|
|0x83 0x04|BLOB Block Start|
|0x83 0x05|BLOB Block Get|
|0x83 0x06|BLOB Information Get|
|0x83 0x07|BLOB Information Status|
|0x83 0x08|Firmware Update Information Get|
|0x83 0x09|Firmware Update Information Status|
|0x83 0x0A|Firmware Update Firmware Metadata Check|
|0x83 0x0B|Firmware Update Firmware Metadata Status|
|0x83 0x0C|Firmware Update Get|
|0x83 0x0D|Firmware Update Start|
|0x83 0x0E|Firmware Update Cancel|
|0x83 0x0F|Firmware Update Apply|
|0x83 0x10|Firmware Update Status|
|0x83 0x11|Firmware Distribution Receivers Add|
|0x83 0x12|Firmware Distribution Receivers Delete All|
|0x83 0x13|Firmware Distribution Receivers Status|
|0x83 0x14|Firmware Distribution Receivers Get|
|0x83 0x15|Firmware Distribution Receivers List|
|0x83 0x16|Firmware Distribution Capabilities Get|
|0x83 0x17|Firmware Distribution Capabilities Status|
|0x83 0x18|Firmware Distribution Get|
|0x83 0x19|Firmware Distribution Start|



Bluetooth SIG Proprietary 

Page 154 of 442 

**Assigned Numbers** / Document 

**2026-06-23** 

|0x83 0x1A|Firmware Distribution Suspend|
|---|---|
|0x83 0x1B|Firmware Distribution Cancel|
|0x83 0x1C|Firmware Distribution Apply|
|0x83 0x1D|Firmware Distribution Status|
|0x83 0x1E|Firmware Distribution Upload Get|
|0x83 0x1F|Firmware Distribution Upload Start|
|0x83 0x20|Firmware Distribution Upload OOB Start|
|0x83 0x21|Firmware Distribution Upload Cancel|
|0x83 0x22|Firmware Distribution Upload Status|
|0x83 0x23|Firmware Distribution Firmware Get|
|0x83 0x24|Firmware Distribution Firmware Get By Index|
|0x83 0x25|Firmware Distribution Firmware Delete|
|0x83 0x26|Firmware Distribution Firmware Delete All|
|0x83 0x27|Firmware Distribution Firmware Status|



##### **4.2.2 Mesh Model Message Opcodes by Name** 

The table below lists the assigned numbers for model message opcodes organized by message name. Last Modified: 2024-02-05 

Filename: assigned_numbers/mesh/mmdl_opcodes.yaml 

|**Message Name**|**Message Opcode**|
|---|---|
|BLOB Block Get|0x83 0x05|
|BLOB Block Start|0x83 0x04|
|BLOB Block Status|0x67|
|BLOB Chunk Transfer|0x66|
|BLOB Information Get|0x83 0x06|
|BLOB Information Status|0x83 0x07|
|BLOB Partial Block Report|0x68|
|BLOB Transfer Cancel|0x83 0x02|
|BLOB Transfer Get|0x83 0x00|
|BLOB Transfer Start|0x83 0x01|
|BLOB Transfer Status|0x83 0x03|
|BRIDGED_SUBNETS_GET|0x80 0xB7|
|BRIDGED_SUBNETS_LIST|0x80 0xB8|
|BRIDGING_TABLE_ADD|0x80 0xB4|



Bluetooth SIG Proprietary 

Page 155 of 442 

**Assigned Numbers** / Document 

**2026-06-23** 

|BRIDGING_TABLE_GET|0x80 0xB9|
|---|---|
|BRIDGING_TABLE_LIST|0x80 0xBA|
|BRIDGING_TABLE_REMOVE|0x80 0xB5|
|BRIDGING_TABLE_SIZE_GET|0x80 0xBB|
|BRIDGING_TABLE_SIZE_STATUS|0x80 0xBC|
|BRIDGING_TABLE_STATUS|0x80 0xB6|
|Config AppKey Add|0x00|
|Config AppKey Delete|0x80 0x00|
|Config AppKey Get|0x80 0x01|
|Config AppKey List|0x80 0x02|
|Config AppKey Status|0x80 0x03|
|Config AppKey Update|0x01|
|Config Beacon Get|0x80 0x09|
|Config Beacon Set|0x80 0x0A|
|Config Beacon Status|0x80 0x0B|
|Config Composition Data Get|0x80 0x08|
|Config Composition Data Status|0x02|
|Config Default TTL Get|0x80 0x0C|
|Config Default TTL Set|0x80 0x0D|
|Config Default TTL Status|0x80 0x0E|
|Config Friend Get|0x80 0x0F|
|Config Friend Set|0x80 0x10|
|Config Friend Status|0x80 0x11|
|Config GATT Proxy Get|0x80 0x12|
|Config GATT Proxy Set|0x80 0x13|
|Config GATT Proxy Status|0x80 0x14|
|Config Heartbeat Publication Get|0x80 0x38|
|Config Heartbeat Publication Set|0x80 0x39|
|Config Heartbeat Publication Status|0x06|
|Config Heartbeat Subscription Get|0x80 0x3A|
|Config Heartbeat Subscription Set|0x80 0x3B|
|Config Heartbeat Subscription Status|0x80 0x3C|
|Config Key Refresh Phase Get|0x80 0x15|
|Config Key Refresh Phase Set|0x80 0x16|
|Config Key Refresh Phase Status|0x80 0x17|
|Config Low Power Node PollTimeout Get<br>Bluetooth SIG Proprietary|0x80 0x2D<br>Pa|



Page 156 of 442 

**Assigned Numbers** / Document 

**2026-06-23** 

|Config Low Power Node PollTimeout Status|0x80 0x2E|
|---|---|
|Config Model App Bind|0x80 0x3D|
|Config Model App Status|0x80 0x3E|
|Config Model App Unbind|0x80 0x3F|
|Config Model Publication Get|0x80 0x18|
|Config Model Publication Set|0x03|
|Config Model Publication Status|0x80 0x19|
|Config Model Publication Virtual Address Set|0x80 0x1A|
|Config Model Subscription Add|0x80 0x1B|
|Config Model Subscription Delete|0x80 0x1C|
|Config Model Subscription Delete All|0x80 0x1D|
|Config Model Subscription Overwrite|0x80 0x1E|
|Config Model Subscription Status|0x80 0x1F|
|Config Model Subscription Virtual Address Add|0x80 0x20|
|Config Model Subscription Virtual Address Delete|0x80 0x21|
|Config Model Subscription Virtual Address Overwrite|0x80 0x22|
|Config NetKey Add|0x80 0x40|
|Config NetKey Delete|0x80 0x41|
|Config NetKey Get|0x80 0x42|
|Config NetKey List|0x80 0x43|
|Config NetKey Status|0x80 0x44|
|Config NetKey Update|0x80 0x45|
|Config Network Transmit Get|0x80 0x23|
|Config Network Transmit Set|0x80 0x24|
|Config Network Transmit Status|0x80 0x25|
|Config Node Identity Get|0x80 0x46|
|Config Node Identity Set|0x80 0x47|
|Config Node Identity Status|0x80 0x48|
|Config Node Reset|0x80 0x49|
|Config Node Reset Status|0x80 0x4A|
|Config Relay Get|0x80 0x26|
|Config Relay Set|0x80 0x27|
|Config Relay Status|0x80 0x28|
|Config SIG Model App Get|0x80 0x4B|
|Config SIG Model App List|0x80 0x4C|
|Config SIG Model Subscription Get<br>Bluetooth SIG Proprietary|0x80 0x29<br>Pa|



Page 157 of 442 

**Assigned Numbers** / Document 

**2026-06-23** 

|Config SIG Model Subscription List|0x80 0x2A|
|---|---|
|Config Vendor Model App Get|0x80 0x4D|
|Config Vendor Model App List|0x80 0x4E|
|Config Vendor Model Subscription Get|0x80 0x2B|
|Config Vendor Model Subscription List|0x80 0x2C|
|DIRECTED_CONTROL_GET|0x80 0x7B|
|DIRECTED_CONTROL_NETWORK_TRANSMIT_GET|0x80 0xAB|
|DIRECTED_CONTROL_NETWORK_TRANSMIT_SET|0x80 0xAC|
|DIRECTED_CONTROL_NETWORK_TRANSMIT_STATUS|0x80 0xAD|
|DIRECTED_CONTROL_RELAY_RETRANSMIT_GET|0x80 0xAE|
|DIRECTED_CONTROL_RELAY_RETRANSMIT_SET|0x80 0xAF|
|DIRECTED_CONTROL_RELAY_RETRANSMIT_STATUS|0x80 0xB0|
|DIRECTED_CONTROL_SET|0x80 0x7C|
|DIRECTED_CONTROL_STATUS|0x80 0x7D|
|DIRECTED_NETWORK_TRANSMIT_GET|0x80 0x99|
|DIRECTED_NETWORK_TRANSMIT_SET|0x80 0x9A|
|DIRECTED_NETWORK_TRANSMIT_STATUS|0x80 0x9B|
|DIRECTED_PATHS_GET|0x80 0xA2|
|DIRECTED_PATHS_STATUS|0x80 0xA3|
|DIRECTED_PUBLISH_POLICY_GET|0x80 0xA4|
|DIRECTED_PUBLISH_POLICY_SET|0x80 0xA5|
|DIRECTED_PUBLISH_POLICY_STATUS|0x80 0xA6|
|DIRECTED_RELAY_RETRANSMIT_GET|0x80 0x9C|
|DIRECTED_RELAY_RETRANSMIT_SET|0x80 0x9D|
|DIRECTED_RELAY_RETRANSMIT_STATUS|0x80 0x9E|
|DISCOVERY_TABLE_CAPABILITIES_GET|0x80 0x81|
|DISCOVERY_TABLE_CAPABILITIES_SET|0x80 0x82|
|DISCOVERY_TABLE_CAPABILITIES_STATUS|0x80 0x83|
|FORWARDING_TABLE_ADD|0x80 0x84|
|FORWARDING_TABLE_DELETE|0x80 0x85|
|FORWARDING_TABLE_DEPENDENTS_ADD|0x80 0x87|
|FORWARDING_TABLE_DEPENDENTS_DELETE|0x80 0x88|
|FORWARDING_TABLE_DEPENDENTS_GET|0x80 0x8A|
|FORWARDING_TABLE_DEPENDENTS_GET_STATUS|0x80 0x8B|
|FORWARDING_TABLE_DEPENDENTS_STATUS|0x80 0x89|
|FORWARDING_TABLE_ENTRIES_COUNT_GET<br>Bluetooth SIG Proprietary|0x80 0x8C<br>Pa|



Page 158 of 442 

**Assigned Numbers** / Document 

**2026-06-23** 

|FORWARDING_TABLE_ENTRIES_COUNT_STATUS|0x80 0x8D|
|---|---|
|FORWARDING_TABLE_ENTRIES_GET|0x80 0x8E|
|FORWARDING_TABLE_ENTRIES_STATUS|0x80 0x8F|
|FORWARDING_TABLE_STATUS|0x80 0x86|
|Firmware Distribution Apply|0x83 0x1C|
|Firmware Distribution Cancel|0x83 0x1B|
|Firmware Distribution Capabilities Get|0x83 0x16|
|Firmware Distribution Capabilities Status|0x83 0x17|
|Firmware Distribution Firmware Delete|0x83 0x25|
|Firmware Distribution Firmware Delete All|0x83 0x26|
|Firmware Distribution Firmware Get|0x83 0x23|
|Firmware Distribution Firmware Get By Index|0x83 0x24|
|Firmware Distribution Firmware Status|0x83 0x27|
|Firmware Distribution Get|0x83 0x18|
|Firmware Distribution Receivers Add|0x83 0x11|
|Firmware Distribution Receivers Delete All|0x83 0x12|
|Firmware Distribution Receivers Get|0x83 0x14|
|Firmware Distribution Receivers List|0x83 0x15|
|Firmware Distribution Receivers Status|0x83 0x13|
|Firmware Distribution Start|0x83 0x19|
|Firmware Distribution Status|0x83 0x1D|
|Firmware Distribution Suspend|0x83 0x1A|
|Firmware Distribution Upload Cancel|0x83 0x21|
|Firmware Distribution Upload Get|0x83 0x1E|
|Firmware Distribution Upload OOB Start|0x83 0x20|
|Firmware Distribution Upload Start|0x83 0x1F|
|Firmware Distribution Upload Status|0x83 0x22|
|Firmware Update Apply|0x83 0x0F|
|Firmware Update Cancel|0x83 0x0E|
|Firmware Update Firmware Metadata Check|0x83 0x0A|
|Firmware Update Firmware Metadata Status|0x83 0x0B|
|Firmware Update Get|0x83 0x0C|
|Firmware Update Information Get|0x83 0x08|
|Firmware Update Information Status|0x83 0x09|
|Firmware Update Start|0x83 0x0D|
|Firmware Update Status<br>Bluetooth SIG Proprietary|0x83 0x10<br>Pa|



Page 159 of 442 

**Assigned Numbers** / Document 

**2026-06-23** 

|Generic Admin Properties Get|0x82 0x2C|
|---|---|
|Generic Admin Properties Status|0x47|
|Generic Admin Property Get|0x82 0x2D|
|Generic Admin Property Set|0x48|
|Generic Admin Property Set Unacknowledged|0x49|
|Generic Admin Property Status|0x4A|
|Generic Battery Get|0x82 0x23|
|Generic Battery Status|0x82 0x24|
|Generic Client Properties Get|0x4F|
|Generic Client Properties Status|0x50|
|Generic Default Transition Time Get|0x82 0x0D|
|Generic Default Transition Time Set|0x82 0x0E|
|Generic Default Transition Time Set Unacknowledged|0x82 0x0F|
|Generic Default Transition Time Status|0x82 0x10|
|Generic Delta Set|0x82 0x09|
|Generic Delta Set Unacknowledged|0x82 0x0A|
|Generic Level Get|0x82 0x05|
|Generic Level Set|0x82 0x06|
|Generic Level Set Unacknowledged|0x82 0x07|
|Generic Level Status|0x82 0x08|
|Generic Location Global Get|0x82 0x25|
|Generic Location Global Set|0x41|
|Generic Location Global Set Unacknowledged|0x42|
|Generic Location Global Status|0x40|
|Generic Location Local Get|0x82 0x26|
|Generic Location Local Set|0x82 0x28|
|Generic Location Local Set Unacknowledged|0x82 0x29|
|Generic Location Local Status|0x82 0x27|
|Generic Manufacturer Properties Get|0x82 0x2A|
|Generic Manufacturer Properties Status|0x43|
|Generic Manufacturer Property Get|0x82 0x2B|
|Generic Manufacturer Property Set|0x44|
|Generic Manufacturer Property Set Unacknowledged|0x45|
|Generic Manufacturer Property Status|0x46|
|Generic Move Set|0x82 0x0B|
|Generic Move Set Unacknowledged<br>Bluetooth SIG Proprietary|0x82 0x0C<br>Pa|



Page 160 of 442 

**Assigned Numbers** / Document 

**2026-06-23** 

|Generic OnOff Get|0x82 0x01|
|---|---|
|Generic OnOff Set|0x82 0x02|
|Generic OnOff Set Unacknowledged|0x82 0x03|
|Generic OnOff Status|0x82 0x04|
|Generic OnPowerUp Get|0x82 0x11|
|Generic OnPowerUp Set|0x82 0x13|
|Generic OnPowerUp Set Unacknowledged|0x82 0x14|
|Generic OnPowerUp Status|0x82 0x12|
|Generic Power Default Get|0x82 0x1B|
|Generic Power Default Set|0x82 0x1F|
|Generic Power Default Set Unacknowledged|0x82 0x20|
|Generic Power Default Status|0x82 0x1C|
|Generic Power Last Get|0x82 0x19|
|Generic Power Last Status|0x82 0x1A|
|Generic Power Level Get|0x82 0x15|
|Generic Power Level Set|0x82 0x16|
|Generic Power Level Set Unacknowledged|0x82 0x17|
|Generic Power Level Status|0x82 0x18|
|Generic Power Range Get|0x82 0x1D|
|Generic Power Range Set|0x82 0x21|
|Generic Power Range Set Unacknowledged|0x82 0x22|
|Generic Power Range Status|0x82 0x1E|
|Generic User Properties Get|0x82 0x2E|
|Generic User Properties Status|0x4B|
|Generic User Property Get|0x82 0x2F|
|Generic User Property Set|0x4C|
|Generic User Property Set Unacknowledged|0x4D|
|Generic User Property Status|0x4E|
|Health Attention Get|0x80 0x04|
|Health Attention Set|0x80 0x05|
|Health Attention Set Unacknowledged|0x80 0x06|
|Health Attention Status|0x80 0x07|
|Health Current Status|0x04|
|Health Fault Clear|0x80 0x2F|
|Health Fault Clear Unacknowledged|0x80 0x30|
|Health Fault Get|0x80 0x31|



Bluetooth SIG Proprietary 

Page 161 of 442 

**Assigned Numbers** / Document 

**2026-06-23** 

|Health Fault Status|0x05|
|---|---|
|Health Fault Test|0x80 0x32|
|Health Fault Test Unacknowledged|0x80 0x33|
|Health Period Get|0x80 0x34|
|Health Period Set|0x80 0x35|
|Health Period Set Unacknowledged|0x80 0x36|
|Health Period Status|0x80 0x37|
|IEC 62386-104 Frame|0x65|
|LARGE_COMPOSITION_DATA_GET|0x80 0x74|
|LARGE_COMPOSITION_DATA_STATUS|0x80 0x75|
|Light CTL Default Get|0x82 0x67|
|Light CTL Default Set|0x82 0x69|
|Light CTL Default Set Unacknowledged|0x82 0x6A|
|Light CTL Default Status|0x82 0x68|
|Light CTL Get|0x82 0x5D|
|Light CTL Set|0x82 0x5E|
|Light CTL Set Unacknowledged|0x82 0x5F|
|Light CTL Status|0x82 0x60|
|Light CTL Temperature Get|0x82 0x61|
|Light CTL Temperature Range Get|0x82 0x62|
|Light CTL Temperature Range Set|0x82 0x6B|
|Light CTL Temperature Range Set Unacknowledged|0x82 0x6C|
|Light CTL Temperature Range Status|0x82 0x63|
|Light CTL Temperature Set|0x82 0x64|
|Light CTL Temperature Set Unacknowledged|0x82 0x65|
|Light CTL Temperature Status|0x82 0x66|
|Light HSL Default Get|0x82 0x7B|
|Light HSL Default Set|0x82 0x7F|
|Light HSL Default Set Unacknowledged|0x82 0x80|
|Light HSL Default Status|0x82 0x7C|
|Light HSL Get|0x82 0x6D|
|Light HSL Hue Get|0x82 0x6E|
|Light HSL Hue Set|0x82 0x6F|
|Light HSL Hue Set Unacknowledged|0x82 0x70|
|Light HSL Hue Status|0x82 0x71|
|Light HSL Range Get<br>Bluetooth SIG Proprietary|0x82 0x7D<br>Pa|



Page 162 of 442 

**Assigned Numbers** / Document 

**2026-06-23** 

|Light HSL Range Set|0x82 0x81|
|---|---|
|Light HSL Range Set Unacknowledged|0x82 0x82|
|Light HSL Range Status|0x82 0x7E|
|Light HSL Saturation Get|0x82 0x72|
|Light HSL Saturation Set|0x82 0x73|
|Light HSL Saturation Set Unacknowledged|0x82 0x74|
|Light HSL Saturation Status|0x82 0x75|
|Light HSL Set|0x82 0x76|
|Light HSL Set Unacknowledged|0x82 0x77|
|Light HSL Status|0x82 0x78|
|Light HSL Target Get|0x82 0x79|
|Light HSL Target Status|0x82 0x7A|
|Light LC Light OnOff Get|0x82 0x99|
|Light LC Light OnOff Set|0x82 0x9A|
|Light LC Light OnOff Set Unacknowledged|0x82 0x9B|
|Light LC Light OnOff Status|0x82 0x9C|
|Light LC Mode Get|0x82 0x91|
|Light LC Mode Set|0x82 0x92|
|Light LC Mode Set Unacknowledged|0x82 0x93|
|Light LC Mode Status|0x82 0x94|
|Light LC OM Get|0x82 0x95|
|Light LC OM Set|0x82 0x96|
|Light LC OM Set Unacknowledged|0x82 0x97|
|Light LC OM Status|0x82 0x98|
|Light LC Property Get|0x82 0x9D|
|Light LC Property Set|0x62|
|Light LC Property Set Unacknowledged|0x63|
|Light LC Property Status|0x64|
|Light Lightness Default Get|0x82 0x55|
|Light Lightness Default Set|0x82 0x59|
|Light Lightness Default Set Unacknowledged|0x82 0x5A|
|Light Lightness Default Status|0x82 0x56|
|Light Lightness Get|0x82 0x4B|
|Light Lightness Last Get|0x82 0x53|
|Light Lightness Last Status|0x82 0x54|
|Light Lightness Linear Get<br>Bluetooth SIG Proprietary|0x82 0x4F<br>Pa|



Page 163 of 442 

**Assigned Numbers** / Document 

**2026-06-23** 

|Light Lightness Linear Set|0x82 0x50|
|---|---|
|Light Lightness Linear Set Unacknowledged|0x82 0x51|
|Light Lightness Linear Status|0x82 0x52|
|Light Lightness Range Get|0x82 0x57|
|Light Lightness Range Set|0x82 0x5B|
|Light Lightness Range Set Unacknowledged|0x82 0x5C|
|Light Lightness Range Status|0x82 0x58|
|Light Lightness Set|0x82 0x4C|
|Light Lightness Set Unacknowledged|0x82 0x4D|
|Light Lightness Status|0x82 0x4E|
|Light xyL Default Get|0x82 0x89|
|Light xyL Default Set|0x82 0x8D|
|Light xyL Default Set Unacknowledged|0x82 0x8E|
|Light xyL Default Status|0x82 0x8A|
|Light xyL Get|0x82 0x83|
|Light xyL Range Get|0x82 0x8B|
|Light xyL Range Set|0x82 0x8F|
|Light xyL Range Set Unacknowledged|0x82 0x90|
|Light xyL Range Status|0x82 0x8C|
|Light xyL Set|0x82 0x84|
|Light xyL Set Unacknowledged|0x82 0x85|
|Light xyL Status|0x82 0x86|
|Light xyL Target Get|0x82 0x87|
|Light xyL Target Status|0x82 0x88|
|MODELS_METADATA_GET|0x80 0x76|
|MODELS_METADATA_STATUS|0x80 0x77|
|ON_DEMAND_PRIVATE_PROXY_GET|0x80 0x69|
|ON_DEMAND_PRIVATE_PROXY_SET|0x80 0x6A|
|ON_DEMAND_PRIVATE_PROXY_STATUS|0x80 0x6B|
|OPCODES_AGGREGATOR_SEQUENCE|0x80 0x72|
|OPCODES_AGGREGATOR_STATUS|0x80 0x73|
|PATH_DISCOVERY_TIMING_CONTROL_GET|0x80 0xA7|
|PATH_DISCOVERY_TIMING_CONTROL_SET|0x80 0xA8|
|PATH_DISCOVERY_TIMING_CONTROL_STATUS|0x80 0xA9|
|PATH_ECHO_INTERVAL_GET|0x80 0x96|
|PATH_ECHO_INTERVAL_SET<br>Bluetooth SIG Proprietary|0x80 0x97<br>Pa|



Page 164 of 442 

**Assigned Numbers** / Document 

**2026-06-23** 

|PATH_ECHO_INTERVAL_STATUS|0x80 0x98|
|---|---|
|PATH_METRIC_GET|0x80 0x7E|
|PATH_METRIC_SET|0x80 0x7F|
|PATH_METRIC_STATUS|0x80 0x80|
|PRIVATE_BEACON_GET|0x80 0x60|
|PRIVATE_BEACON_SET|0x80 0x61|
|PRIVATE_BEACON_STATUS|0x80 0x62|
|PRIVATE_GATT_PROXY_GET|0x80 0x63|
|PRIVATE_GATT_PROXY_SET|0x80 0x64|
|PRIVATE_GATT_PROXY_STATUS|0x80 0x65|
|PRIVATE_NODE_IDENTITY_GET|0x80 0x66|
|PRIVATE_NODE_IDENTITY_SET|0x80 0x67|
|PRIVATE_NODE_IDENTITY_STATUS|0x80 0x68|
|RSSI_THRESHOLD_GET|0x80 0x9F|
|RSSI_THRESHOLD_SET|0x80 0xA0|
|RSSI_THRESHOLD_STATUS|0x80 0xA1|
|Remote Provisioning Extended Scan Report|0x80 0x57|
|Remote Provisioning Extended Scan Start|0x80 0x56|
|Remote Provisioning Link Close|0x80 0x5A|
|Remote Provisioning Link Get|0x80 0x58|
|Remote Provisioning Link Open|0x80 0x59|
|Remote Provisioning Link Report|0x80 0x5C|
|Remote Provisioning Link Status|0x80 0x5B|
|Remote Provisioning PDU Outbound Report|0x80 0x5E|
|Remote Provisioning PDU Report|0x80 0x5F|
|Remote Provisioning PDU Send|0x80 0x5D|
|Remote Provisioning Scan Capabilities Get|0x80 0x4F|
|Remote Provisioning Scan Capabilities Status|0x80 0x50|
|Remote Provisioning Scan Get|0x80 0x51|
|Remote Provisioning Scan Report|0x80 0x55|
|Remote Provisioning Scan Start|0x80 0x52|
|Remote Provisioning Scan Status|0x80 0x54|
|Remote Provisioning Scan Stop|0x80 0x53|
|SAR_RECEIVER_GET|0x80 0x6F|
|SAR_RECEIVER_SET|0x80 0x70|
|SAR_RECEIVER_STATUS<br>Bluetooth SIG Proprietary|0x80 0x71<br>Pa|



Page 165 of 442 

**Assigned Numbers** / Document 

**2026-06-23** 

|SAR_TRANSMITTER_GET|0x80 0x6C|
|---|---|
|SAR_TRANSMITTER_SET|0x80 0x6D|
|SAR_TRANSMITTER_STATUS|0x80 0x6E|
|SOLICITATION_PDU_RPL_ITEM_CLEAR|0x80 0x78|
|SOLICITATION_PDU_RPL_ITEM_CLEAR_UNACKNOWLEDGED|0x80 0x79|
|SOLICITATION_PDU_RPL_ITEM_STATUS|0x80 0x7A|
|SUBNET_BRIDGE_GET|0x80 0xB1|
|SUBNET_BRIDGE_SET|0x80 0xB2|
|SUBNET_BRIDGE_STATUS|0x80 0xB3|
|Scene Delete|0x82 0x9E|
|Scene Delete Unacknowledged|0x82 0x9F|
|Scene Get|0x82 0x41|
|Scene Recall|0x82 0x42|
|Scene Recall Unacknowledged|0x82 0x43|
|Scene Register Get|0x82 0x44|
|Scene Register Status|0x82 0x45|
|Scene Status|0x5E|
|Scene Store|0x82 0x46|
|Scene Store Unacknowledged|0x82 0x47|
|Scheduler Action Get|0x82 0x48|
|Scheduler Action Set|0x60|
|Scheduler Action Set Unacknowledged|0x61|
|Scheduler Action Status|0x5F|
|Scheduler Get|0x82 0x49|
|Scheduler Status|0x82 0x4A|
|Sensor Cadence Get|0x82 0x34|
|Sensor Cadence Set|0x55|
|Sensor Cadence Set Unacknowledged|0x56|
|Sensor Cadence Status|0x57|
|Sensor Column Get|0x82 0x32|
|Sensor Column Status|0x53|
|Sensor Descriptor Get|0x82 0x30|
|Sensor Descriptor Status|0x51|
|Sensor Get|0x82 0x31|
|Sensor Series Get|0x82 0x33|
|Sensor Series Status|0x54|



Bluetooth SIG Proprietary 

Page 166 of 442 

**Assigned Numbers** / Document 

**2026-06-23** 

|Sensor Setting Get|0x82 0x36|
|---|---|
|Sensor Setting Set|0x59|
|Sensor Setting Set Unacknowledged|0x5A|
|Sensor Setting Status|0x5B|
|Sensor Settings Get|0x82 0x35|
|Sensor Settings Status|0x58|
|Sensor Status|0x52|
|TAI-UTC Delta Get|0x82 0x3E|
|TAI-UTC Delta Set|0x82 0x3F|
|TAI-UTC Delta Status|0x82 0x40|
|TWO_WAY_PATH_GET|0x80 0x93|
|TWO_WAY_PATH_SET|0x80 0x94|
|TWO_WAY_PATH_STATUS|0x80 0x95|
|Time Get|0x82 0x37|
|Time Role Get|0x82 0x38|
|Time Role Set|0x82 0x39|
|Time Role Status|0x82 0x3A|
|Time Set|0x5C|
|Time Status|0x5D|
|Time Zone Get|0x82 0x3B|
|Time Zone Set|0x82 0x3C|
|Time Zone Status|0x82 0x3D|
|WANTED_LANES_GET|0x80 0x90|
|WANTED_LANES_SET|0x80 0x91|
|WANTED_LANES_STATUS|0x80 0x92|
|this opcode is not used|0x80 0xAA|



Bluetooth SIG Proprietary 

Page 167 of 442 

**Assigned Numbers** / Document 

**2026-06-23** 

#### **4.3 Mesh Protocol** 

The following section includes the assigned numbers used in the Mesh Protocol specification [17] [18]. 

##### **4.3.1 Mesh Beacon Types** 

The table below lists the assigned numbers for beacon types. 

Last Modified: 2024-02-05 

Filename: assigned_numbers/mesh/mesh_beacon_types.yaml 

|**Value**|**Definition**|
|---|---|
|0x00|Unprovisioned Device beacon|
|0x01|Secure Network beacon|
|0x02|Mesh Private beacon|



##### **4.3.2 Mesh Transport Control Message Opcodes** 

The table below lists the assigned numbers for Transport Control message opcodes. 

Last Modified: 2024-02-05 

Filename: assigned_numbers/mesh/mesh_transport_control_opcodes.yaml 

|**Opcode**|**Name**|**Description**|
|---|---|---|
|0x00|–|Reserved for the lower transport layer.|
|0x01|Friend Poll|Sent by a Low Power node to its<br>Friend node to request any messages<br>that it has stored for the Low Power<br>node.|
|0x02|Friend Update|Sent by a Friend node to a Low Power<br>node to inform it about security<br>updates.|
|0x03|Friend Request|Sent by a Low Power node to the<br>all-friends fixed group address to<br>initiate a search for a friend.|
|0x04|Friend Offer|Sent by a Friend node to a Low Power<br>node to offer to become its friend.|
|0x05|Friend Clear|Sent to a Friend node to inform a<br>previous friend of a Low Power node<br>about the removal of a friendship.|
|0x06|Friend Clear Confirm|Sent from a previous friend to the<br>Friend node to confirm that a prior<br>friend relationship has been removed.|
|0x07|Friend Subscription List Add|Sent to a Friend node to add one or<br>more addresses to the Friend<br>Subscription List.|



Bluetooth SIG Proprietary 

Page 168 of 442 

**Assigned Numbers** / Document 

**2026-06-23** 

|0x08|Friend Subscription List Remove|Sent to a Friend node to remove one<br>or more addresses from the Friend<br>Subscription List.|
|---|---|---|
|0x09|Friend Subscription List Confirm|Sent by a Friend node to confirm<br>Friend Subscription List updates.|
|0x0A|Heartbeat|Sent by a node to let other nodes<br>determine topology of a subnet.|
|0x0B|PATH_REQUEST|Sent by a Path Origin or by a Directed<br>Relay node to discover a path to a<br>destination.|
|0x0C|PATH_REPLY|Sent by a Path Target or by a Directed<br>Relay node to establish a path from a<br>Path Origin to a Path Target.|
|0x0D|PATH_CONFIRMATION|Sent by a Path Origin or by a Directed<br>Relay node to confirm that a two-way<br>path has been established from the<br>Path Origin to a Path Target.|
|0x0E|PATH_ECHO_REQUEST|Sent by a Path Origin to validate a<br>path from the Path Origin to a<br>destination.|
|0x0F|PATH_ECHO_REPLY|Sent by a Path Target in order to<br>confirm that a path exists from a Path<br>Origin to the destination.|
|0x10|DEPENDENT_NODE_UPDATE|Sent by a path endpoint or a Directed<br>Relay node to notify nodes in a subnet<br>that element addresses of a<br>dependent node are to be added to or<br>removed from the Forwarding Table.|
|0x11|PATH_REQUEST_SOLICITATION|Sent by a Directed Forwarding node<br>or a Configuration Manager to solicit<br>the discovery of paths toward unicast<br>addresses, group addresses, or virtual<br>addresses.|



##### **4.3.3 Mesh Provisioning PDU Types** 

The table below lists the assigned numbers for Provisioning PDU types. 

Last Modified: 2024-02-05 

Filename: assigned_numbers/mesh/mesh_provisioning_pdu_types.yaml 

|**Value**|**Name**|**Description**|
|---|---|---|
|0x00|Provisioning Invite|Invites an unprovisioned device (the<br>intended Provisionee) to join a mesh<br>network|



Bluetooth SIG Proprietary 

Page 169 of 442 

**Assigned Numbers** / Document 

**2026-06-23** 

|0x01|Provisioning Capabilities|Indicates the capabilities of the<br>Provisionee|
|---|---|---|
|0x02|Provisioning Start|Indicates the provisioning method<br>selected by the Provisioner based on<br>the capabilities of the Provisionee|
|0x03|Provisioning Public Key|Contains the Public Key of the<br>Provisionee or the Provisioner|
|0x04|Provisioning Input Complete|Indicates that the user has completed<br>inputting a value|
|0x05|Provisioning Confirmation|Contains the provisioning confirmation<br>value of the Provisionee or the<br>Provisioner|
|0x06|Provisioning Random|Contains the provisioning random<br>value of the Provisionee or the<br>Provisioner|
|0x07|Provisioning Data|Includes the assigned unicast address<br>of the primary element, a network key,<br>NetKey Index, Flags, and the IV Index|
|0x08|Provisioning Complete|Indicates that provisioning is complete|
|0x09|Provisioning Failed|Indicates that provisioning was<br>unsuccessful|
|0x0A|Provisioning Record Request|Indicates a request to retrieve a<br>provisioning record fragment from the<br>Provisionee|
|0x0B|Provisioning Record Response|Contains a provisioning record<br>fragment or an error status, sent in<br>response to a Provisioning Record<br>Request|
|0x0C|Provisioning Records Get|Indicates a request to retrieve the list<br>of IDs of the provisioning records that<br>the Provisionee supports.|
|0x0D|Provisioning Records List|Contains the list of IDs of the<br>provisioning records that the<br>Provisionee supports.|



##### **4.3.4 Mesh Proxy PDU Types** 

The table below lists the assigned numbers for Proxy PDU types. 

###### Last Modified: 2024-02-05 

Filename: assigned_numbers/mesh/mesh_proxy_pdu_types.yaml 

|**Value**|**Name**|**Description**|
|---|---|---|
|0x00|Network PDU|The message is a Network PDU|
|0x01|Mesh Beacon|The message is a mesh beacon|



Bluetooth SIG Proprietary 

Page 170 of 442 

**Assigned Numbers** / Document 

**2026-06-23** 

|0x02|Proxy Configuration|The message is a proxy configuration|
|---|---|---|
|||message|
|0x03|Provisioning PDU|The message is a Provisioning PDU|



##### **4.3.5 Mesh Health Fault IDs** 

This section lists the Health Fault IDs assigned to the health models. 

###### **4.3.5.1 Mesh Health Fault IDs by Value** 

The table below lists the Health Fault IDs organized by fault ID value. 

###### Last Modified: 2024-02-05 

Filename: assigned_numbers/mesh/mesh_health_faults.yaml 

|**Value**|**Name**|
|---|---|
|0x00|No Fault|
|0x01|Battery Low Warning|
|0x02|Battery Low Error|
|0x03|Supply Voltage Too Low Warning|
|0x04|Supply Voltage Too Low Error|
|0x05|Supply Voltage Too High Warning|
|0x06|Supply Voltage Too High Error|
|0x07|Power Supply Interrupted Warning|
|0x08|Power Supply Interrupted Error|
|0x09|No Load Warning|
|0x0A|No Load Error|
|0x0B|Overload Warning|
|0x0C|Overload Error|
|0x0D|Overheat Warning|
|0x0E|Overheat Error|
|0x0F|Condensation Warning|
|0x10|Condensation Error|
|0x11|Vibration Warning|
|0x12|Vibration Error|
|0x13|Configuration Warning|
|0x14|Configuration Error|
|0x15|Element Not Calibrated Warning|
|0x16|Element Not Calibrated Error|



Bluetooth SIG Proprietary 

Page 171 of 442 

**Assigned Numbers** / Document 

**2026-06-23** 

|0x17|Memory Warning|
|---|---|
|0x18|Memory Error|
|0x19|Self-Test Warning|
|0x1A|Self-Test Error|
|0x1B|Input Too Low Warning|
|0x1C|Input Too Low Error|
|0x1D|Input Too High Warning|
|0x1E|Input Too High Error|
|0x1F|Input No Change Warning|
|0x20|Input No Change Error|
|0x21|Actuator Blocked Warning|
|0x22|Actuator Blocked Error|
|0x23|Housing Opened Warning|
|0x24|Housing Opened Error|
|0x25|Tamper Warning|
|0x26|Tamper Error|
|0x27|Device Moved Warning|
|0x28|Device Moved Error|
|0x29|Device Dropped Warning|
|0x2A|Device Dropped Error|
|0x2B|Overflow Warning|
|0x2C|Overflow Error|
|0x2D|Empty Warning|
|0x2E|Empty Error|
|0x2F|Internal Bus Warning|
|0x30|Internal Bus Error|
|0x31|Mechanism Jammed Warning|
|0x32|Mechanism Jammed Error|
|0x80‐0xFF|Vendor Specific Warning / Error|



###### **4.3.5.2 Mesh Health Fault IDs by Name** 

The table below lists the Health Fault IDs organized by fault name. 

Last Modified: 2024-02-05 

Filename: assigned_numbers/mesh/mesh_health_faults.yaml 

Bluetooth SIG Proprietary 

Page 172 of 442 

**Assigned Numbers** / Document 

**2026-06-23** 

|**Name**|**Value**|
|---|---|
|Actuator Blocked Error|0x22|
|Actuator Blocked Warning|0x21|
|Battery Low Error|0x02|
|Battery Low Warning|0x01|
|Condensation Error|0x10|
|Condensation Warning|0x0F|
|Configuration Error|0x14|
|Configuration Warning|0x13|
|Device Dropped Error|0x2A|
|Device Dropped Warning|0x29|
|Device Moved Error|0x28|
|Device Moved Warning|0x27|
|Element Not Calibrated Error|0x16|
|Element Not Calibrated Warning|0x15|
|Empty Error|0x2E|
|Empty Warning|0x2D|
|Housing Opened Error|0x24|
|Housing Opened Warning|0x23|
|Input No Change Error|0x20|
|Input No Change Warning|0x1F|
|Input Too High Error|0x1E|
|Input Too High Warning|0x1D|
|Input Too Low Error|0x1C|
|Input Too Low Warning|0x1B|
|Internal Bus Error|0x30|
|Internal Bus Warning|0x2F|
|Mechanism Jammed Error|0x32|
|Mechanism Jammed Warning|0x31|
|Memory Error|0x18|
|Memory Warning|0x17|
|No Fault|0x00|
|No Load Error|0x0A|
|No Load Warning|0x09|
|Overflow Error|0x2C|
|Overflow Warning<br>Bluetooth SIG Proprietary|0x2B<br>Page 173|



Page 173 of 442 

**Assigned Numbers** / Document 

**2026-06-23** 

|Overheat Error|0x0E|
|---|---|
|Overheat Warning|0x0D|
|Overload Error|0x0C|
|Overload Warning|0x0B|
|Power Supply Interrupted Error|0x08|
|Power Supply Interrupted Warning|0x07|
|Self-Test Error|0x1A|
|Self-Test Warning|0x19|
|Supply Voltage Too High Error|0x06|
|Supply Voltage Too High Warning|0x05|
|Supply Voltage Too Low Error|0x04|
|Supply Voltage Too Low Warning|0x03|
|Tamper Error|0x26|
|Tamper Warning|0x25|
|Vendor Specific Warning / Error|0x80‐0xFF|
|Vibration Error|0x12|
|Vibration Warning|0x11|



##### **4.3.6 Mesh Metadata Identifiers** 

The table below lists the assigned numbers for metadata identifiers. 

###### Last Modified: 2024-02-05 

Filename: assigned_numbers/mesh/mesh_metadata_ids.yaml 

|**Value**|**Identifier**|
|---|---|
|0x0000|Health Tests Information|
|0x0001|Sensor Properties|
|0x0002|Light Purpose|
|0x0003|Light Lightness Range|
|0x0004|Light CTL Temperature Range|
|0x0005|Light HSL Hue Range|
|0x0006|Light HSL Saturation Range|
|0x0007|Clock Accuracy|
|0x0008|Timekeeping Reserve|



Bluetooth SIG Proprietary 

Page 174 of 442 

**Assigned Numbers** / Document 

**2026-06-23** 

#### **4.4 Mesh Model** 

The following section includes the assigned numbers used in the Mesh Model specification [16]. 

##### **4.4.1 Light Purpose** 

The table below lists the assigned numbers for Light Purpose metadata. 

Last Modified: 2024-02-05 

Filename: assigned_numbers/mesh/mmdl_light_purposes.yaml 

|**Value**|**Definition**|
|---|---|
|0x0000|Uplight|
|0x0001|Uplight Left|
|0x0002|Uplight Center|
|0x0003|Uplight Right|
|0x0004|Downlight|
|0x0005|Downlight Left|
|0x0006|Downlight Center|
|0x0007|Downlight Right|
|0x0008|Inside|
|0x0009|Outside|
|0x000A|Backlight|
|0x000B|Floodlight|
|0x000C|Tasklight|
|0x000D|Tasklight Left|
|0x000E|Tasklight Center|
|0x000F|Tasklight Right|
|0x0010|Warming Light|
|0x0011|Emergency Light|
|0x0012|Night Light|
|0x0013|Indicator Light|
|0x0014|Undercabinet Light|
|0x0015|Accent Light|
|0x0016|Strip Light|
|0x0017|Troffer Light|
|0x0018|High Bay Light|
|0x0019|Wall Pack Light|



Bluetooth SIG Proprietary 

Page 175 of 442 

**Assigned Numbers** / Document 

**2026-06-23** 

## **5 Service Discover** **<u>y</u>** 

#### **5.1 Attribute Identifiers** 

##### **5.1.1 Advanced Audio Distribution Profile (A2DP)** 

Applicable to Service Class UUIDs: 

- Audio Source: 0x110A 

- Audio Sink: 0x110B 

See also Section 6.5. 

Last Modified: 2024-03-25 

Filename: assigned_numbers/service_discovery/attribute_ids/a2dp.yaml 

|**Attribute ID**|**Attribute Name**|
|---|---|
|0x0311|Supported Features|



##### **5.1.2 Audio/Video Remote Control Profile (AVRCP)** 

Applicable to Service Class UUIDs: 

- A/V Remote Control Target: 0x110C 

- A/V Remote Control: 0x110E 

- A/V Remote Control Controller: 0x110F 

See also Section 6.4. 

Last Modified: 2024-03-25 

Filename: assigned_numbers/service_discovery/attribute_ids/avrcp.yaml 

|**Attribute ID**|**Attribute Name**|
|---|---|
|0x0311|Supported Features|



##### **5.1.3 Basic Imaging Profile (BIP)** 

Applicable to Service Class UUIDs: 

- Imaging Responder: 0x111B 

- Imaging Automatic Archive: 0x111C 

- Imaging Referenced Objects: 0x111D 

Last Modified: 2024-03-25 

Filename: assigned_numbers/service_discovery/attribute_ids/bip.yaml 

|**Attribute ID**|**Attribute Name**|
|---|---|
|0x0200|GoepL2capPsm (BIP v1.1 and later)|



Bluetooth SIG Proprietary 

Page 176 of 442 

**Assigned Numbers** / Document 

**2026-06-23** 

|0x0310|Supported Capabilities|
|---|---|
|0x0311|Supported Features|
|0x0312|Supported Functions|
|0x0313|Total Imaging Data Capacity|



##### **5.1.4 Basic Printing Profile (BPP)** 

Applicable to Service Class UUIDs: 

- Direct Printing: 0x1118 

- ReferencePrinting: 0x1119 

- DirectPrintingReferencedObjectsService: 0x1120 

- ReflectedUI: 0x1121 

- PrintingStatus: 0x1123 

###### Last Modified: 2024-02-05 

Filename: assigned_numbers/service_discovery/attribute_ids/bpp.yaml 

|**Attribute ID**|**Attribute Name**|
|---|---|
|0x0350|Document Formats Supported|
|0x0352|Character Repertoires Supported|
|0x0354|XHTML-Print Image Formats Supported|
|0x0356|Color Supported|
|0x0358|1284ID|
|0x035A|Printer Name|
|0x035C|Printer Location|
|0x035E|Duplex Supported|
|0x0360|Media Types Supported|
|0x0362|MaxMediaWidth|
|0x0364|MaxMediaLength|
|0x0366|Enhanced Layout Supported|
|0x0368|RUI Formats Supported|
|0x0370|Reference Printing RUI Supported|
|0x0372|Direct Printing RUI Supported|
|0x0374|Reference Printing Top URL|
|0x0376|Direct Printing Top URL|
|0x0378|Printer Admin RUI Top URL|
|0x037A|Device Name|



Bluetooth SIG Proprietary 

Page 177 of 442 

**Assigned Numbers** / Document 

**2026-06-23** 

##### **5.1.5 Bluetooth Core Specification: Universal Attributes** 

The following attribute IDs have the same meaning for all services. 

See Bluetooth Core Specification [Vol 3] Part B, Section 5.1 [4]. 

Last Modified: 2024-02-05 

Filename: assigned_numbers/service_discovery/attribute_ids/universal_attributes.yaml 

|**Attribute ID**|**Attribute Name**|
|---|---|
|0x0000|ServiceRecordHandle|
|0x0001|ServiceClassIDList|
|0x0002|ServiceRecordState|
|0x0003|ServiceID|
|0x0004|ProtocolDescriptorList|
|0x0005|BrowseGroupList|
|0x0006|LanguageBaseAttributeIDList|
|0x0007|ServiceInfoTimeToLive|
|0x0008|ServiceAvailability|
|0x0009|BluetoothProfileDescriptorList|
|0x000A|DocumentationURL|
|0x000B|ClientExecutableURL|
|0x000C|IconURL|
|0x000D|AdditionalProtocolDescriptorLists|



##### **5.1.6 Bluetooth Core Specification: Service Discovery Service** 

Applicable to Service Class UUIDs: 

- ServiceDiscoveryServerServiceClassID: 0x1000 

See Bluetooth Core Specification [Vol 3] Part B, Section 5.2 [4]. 

Last Modified: 2024-02-05 

Filename: assigned_numbers/service_discovery/attribute_ids/sdp.yaml 

|**Attribute ID**|**Attribute Name**|
|---|---|
|0x0200|VersionNumberList|
|0x0201|ServiceDatabaseState|



Bluetooth SIG Proprietary 

Page 178 of 442 

**Assigned Numbers** / Document 

**2026-06-23** 

##### **5.1.7 Bluetooth Core Specification: Browse Group Descriptor Service** 

Applicable to Service Class UUIDs: 

- BrowseGroupDescriptorServiceClassID: 0x1001 

See Bluetooth Core Specification [Vol 3] Part B, Section 5.3 [4]. 

Last Modified: 2024-02-05 

Filename: assigned_numbers/service_discovery/attribute_ids/browse_group_descriptor_service.yaml 

|**Attribute ID**|**Attribute Name**|
|---|---|
|0x0200|GroupID|



##### **5.1.8 Calendar Tasks and Notes (CTN)** 

Applicable to Service Class UUIDs: 

- CTN Access Service: 0x113C 

- CTN Notification Service: 0x113D 

Last Modified: 2024-03-25 

Filename: assigned_numbers/service_discovery/attribute_ids/calendar_tasks_and_notes.yaml 

|**Attribute ID**|**Attribute Name**|
|---|---|
|0x0315|CTN InstanceID|
|0x0317|CTN Supported Features|



##### **5.1.9 Cordless Telephony Profile (CTP)** 

Applicable to Service Class UUIDs: 

- CordlessTelephony: 0x1109 

Last Modified: 2024-02-05 

Filename: assigned_numbers/service_discovery/attribute_ids/cordless_telephony_profile.yaml 

|**Attribute ID**|**Attribute Name**|
|---|---|
|0x0301|External Network|



##### **5.1.10 Device Identification Profile (DID)** 

Applicable to Service Class UUIDs: 

- PnPInformation: 0x1200 

Last Modified: 2024-02-05 

Bluetooth SIG Proprietary 

Page 179 of 442 

**Assigned Numbers** / Document 

**2026-06-23** 

Filename: assigned_numbers/service_discovery/attribute_ids/device_identification_profile.yaml 

|**Attribute ID**|**Attribute Name**|
|---|---|
|0x0200|SpecificationID|
|0x0201|VendorID|
|0x0202|ProductID|
|0x0203|Version|
|0x0204|PrimaryRecord|
|0x0205|VendorIDSource|



##### **5.1.11 Dial-Up Networking Profile (DUN)** 

Applicable to Service Class UUIDs: 

- Dial-Up Networking: 0x1103 

Last Modified: 2024-03-25 

Filename: assigned_numbers/service_discovery/attribute_ids/dun.yaml 

|**Attribute ID**|**Attribute Name**|
|---|---|
|0x0305|Audio Feedback Support|



##### **5.1.12 FAX Profile (FAX)** 

Applicable to Service Class UUIDs: 

- Fax: 0x1111 

Last Modified: 2024-02-05 

Filename: assigned_numbers/service_discovery/attribute_ids/fax_profile.yaml 

|**Attribute ID**|**Attribute Name**|
|---|---|
|0x0302|Fax Class 1 Support|
|0x0303|Fax Class 2.0 Support|
|0x0304|Fax Class 2 Support(vendor-specific class)|
|0x0305|Audio Feedback Support|



##### **5.1.13 File Transfer Profile (FTP)** 

Applicable to Service Class UUIDs: 

- OBEX File Transfer: 0x1106 

Last Modified: 2024-02-05 

Bluetooth SIG Proprietary 

Page 180 of 442 

**Assigned Numbers** / Document 

**2026-06-23** 

Filename: assigned_numbers/service_discovery/attribute_ids/file_transfer_profile.yaml 

|**Attribute ID**|**Attribute Name**|
|---|---|
|0x0200|GoepL2capPsm (FTP v1.2 and later)|



##### **5.1.14 Global Navigation Satellite System Profile (GNSS)** 

Applicable to Service Class UUIDs: 

- GNSS_Server: 0x1136 

Last Modified: 2024-02-05 

Filename: assigned_numbers/service_discovery/attribute_ids/gnss.yaml 

|**Attribute ID**|**Attribute Name**|
|---|---|
|0x0200|SupportedFeatures|



##### **5.1.15 Hands-Free Profile (HFP)** 

Applicable to Service Class UUIDs: 

- Hands-Free: 0x111E 

- AG Hands-Free: 0x111F 

See also Section 6.10. 

Last Modified: 2024-02-05 

Filename: assigned_numbers/service_discovery/attribute_ids/hands_free_profile.yaml 

|**Attribute ID**|**Attribute Name**|
|---|---|
|0x0301|Network|
|0x0311|SupportedFeatures|



##### **5.1.16 Hardcopy Replacement Profile (HCRP)** 

Applicable to Service Class UUIDs: 

- HCR_Print: 0x1126 

- HCR_Scan: 0x1127 

Last Modified: 2024-02-05 

Filename: assigned_numbers/service_discovery/attribute_ids/hardcopy_replacement_profile.yaml 

|**Attribute ID**|**Attribute Name**|
|---|---|
|0x0300|1284ID|
|0x0302|Device Name|



Bluetooth SIG Proprietary 

Page 181 of 442 

**Assigned Numbers** / Document 

**2026-06-23** 

|0x0304|Friendly Name|
|---|---|
|0x0306|Device Location|



##### **5.1.17 Headset Profile (HSP)** 

Applicable to Service Class UUIDs: 

- Headset: 0x1108 

- Headset Audio Gateway (AG): 0x1112 

- Headset – HS: 0x1131 

Last Modified: 2024-02-05 

Filename: assigned_numbers/service_discovery/attribute_ids/headset_profile.yaml 

|**Attribute ID**|**Attribute Name**|
|---|---|
|0x0302|Remote Audio Volume Control|



##### **5.1.18 Health Device Profile (HDP)** 

Applicable to Service Class UUIDs: 

- HDP Source: 0x1401 

- HDP Sink: 0x1402 

See also Section 6.8. 

Last Modified: 2024-03-25 

Filename: assigned_numbers/service_discovery/attribute_ids/health_device_profile.yaml 

|**Attribute ID**|**Attribute Name**|
|---|---|
|0x0200|Supported Features|
|0x0301|Data Exchange Specification|
|0x0302|MCAP Supported Procedures|



##### **5.1.19 Human Interface Device Profile (HID)** 

Applicable to Service Class UUIDs: 

- HID: 0x1124 

Last Modified: 2024-03-25 

Filename: assigned_numbers/service_discovery/attribute_ids/human_interface_device_profile.yaml 

|**Attribute ID**|**Attribute Name**|
|---|---|
|0x0200|HIDDeviceReleaseNumber (Deprecated in HID 1.1)|



Bluetooth SIG Proprietary 

Page 182 of 442 

**Assigned Numbers** / Document 

**2026-06-23** 

|0x0201|HIDParserVersion|
|---|---|
|0x0202|HIDDeviceSubclass|
|0x0203|HIDCountryCode|
|0x0204|HIDVirtualCable|
|0x0205|HIDReconnectInitiate|
|0x0206|HIDDescriptorList|
|0x0207|HIDLANGIDBaseList|
|0x0208|HIDSDPDisable (Deprecated in HID 1.1)|
|0x0209|HIDBatteryPower|
|0x020A|HIDRemoteWake|
|0x020B|HIDProfileVersion|
|0x020C|HIDSupervisionTimeout|
|0x020D|HIDNormallyConnectable|
|0x020E|HIDBootDevice|
|0x020F|HIDSSRHostMaxLatency|
|0x0210|HIDSSRHostMinTimeout|



##### **5.1.20 Interoperability Requirements for Bluetooth technology as a WAP Bearer (WAP)** 

Applicable to Service Class UUIDs: 

- WAP: 0x1113 

- WAP_CLIENT: 0x1114 

Last Modified: 2024-02-05 

Filename: assigned_numbers/service_discovery/attribute_ids/interoperability_requirements.yaml 

|**Attribute ID**|**Attribute Name**|
|---|---|
|0x0306|NetworkAddress|
|0x0307|WAPGateway|
|0x0308|HomePageURL|
|0x0309|WAPStackType|



##### **5.1.21 Message Access Profile (MAP)** 

Applicable to Service Class UUIDs: 

- Message Access Server: 0x1132 

- Message Notification Server: 0x1133 

See also Section 6.9. 

Bluetooth SIG Proprietary 

Page 183 of 442 

**Assigned Numbers** / Document 

**2026-06-23** 

Last Modified: 2024-02-05 

Filename: assigned_numbers/service_discovery/attribute_ids/message_access_profile.yaml 

|**Attribute ID**|**Attribute Name**|
|---|---|
|0x0200|GoepL2capPsm (MAP v1.2 and later)|
|0x0315|MASInstanceID|
|0x0316|SupportedMessageTypes|
|0x0317|MapSupportedFeatures (MAP v1.2 and later)|



##### **5.1.22 Multi-Profile Specification (MPS)** 

Applicable to Service Class UUIDs: 

- MPS: 0x113B 

Last Modified: 2024-03-25 

Filename: assigned_numbers/service_discovery/attribute_ids/multi_profile.yaml 

|**Attribute ID**|**Attribute Name**|
|---|---|
|0x0200|Supported Scenarios MPSD|
|0x0201|Supported Scenarios MPMD|
|0x0202|Supported Profile and Protocol Dependecies|



##### **5.1.23 Object Push Profile (OPP)** 

Applicable to Service Class UUIDs: 

- OBEXObjectPush: 0x1105 

Last Modified: 2024-02-05 

Filename: assigned_numbers/service_discovery/attribute_ids/object_push_profile.yaml 

|**Attribute ID**|**Attribute Name**|
|---|---|
|0x0200|GoepL2capPsm (OPP v1.2 and later)|
|0x0300|Service Version|
|0x0303|Supported Formats List|



##### **5.1.24 Personal Area Network Profile (PAN)** 

Applicable to Service Class UUIDs: 

- PANU: 0x1115 

- NAP: 0x1116 

- GN: 0x1117 

Bluetooth SIG Proprietary 

Page 184 of 442 

**Assigned Numbers** / Document 

**2026-06-23** 

Last Modified: 2024-03-25 

Filename: assigned_numbers/service_discovery/attribute_ids/pan_profile.yaml 

|**Attribute ID**|**Attribute Name**|
|---|---|
|0x0200|IpSubnet (Not used in PAN v1.0)|
|0x030A|Security Description|
|0x030B|NetAccessType|
|0x030C|MaxNetAccessRate|
|0x030D|IPv4Subnet|
|0x030E|IPv6Subnet|



##### **5.1.25 Phone Book Access Profile (PBAP)** 

Applicable to Service Class UUIDs: 

- Phonebook Access Client: 0x112E 

- Phonebook Access Server: 0x112F 

Last Modified: 2024-03-25 

Filename: assigned_numbers/service_discovery/attribute_ids/phone_book_access_profile.yaml 

|**Attribute ID**|**Attribute Name**|
|---|---|
|0x0200|GoepL2capPsm (PBAP v1.2 and later)|
|0x0314|Supported Repositories|
|0x0317|PbapSupportedFeatures (PBAP v1.2 and later)|



##### **5.1.26 Synchronization Profile (SYNC)** 

Applicable to Service Class UUIDs: 

- IrMCSync: 0x1104 

Last Modified: 2024-02-05 

Filename: assigned_numbers/service_discovery/attribute_ids/synchronization_profile.yaml 

|**Attribute ID**|**Attribute Name**|
|---|---|
|0x0301|Supported Data Stores List|



#### **5.2 Attribute ID Offsets for Strings** 

Last Modified: 2024-02-05 

Filename: assigned_numbers/service_discovery/attribute_id_offsets_for_strings.yaml 

Bluetooth SIG Proprietary 

Page 185 of 442 

**Assigned Numbers** / Document 

**2026-06-23** 

|**Attribute ID**<br>**Offset**|**Attribute ID Offset Name**|
|---|---|
|0x0000|ServiceName|
|0x0001|ServiceDescription|
|0x0002|ProviderName|



#### **5.3 Protocol Parameters** 

Last Modified: 2024-02-05 

Filename: assigned_numbers/service_discovery/protocol_parameters.yaml 

|**Protocol**|**Parameter Name**|**Parameter Index**|
|---|---|---|
|L2CAP|PSM|1|
|RFCOMM|Channel|1|
|TCP|Port|1|
|UDP|Port|1|
|BNEP|Version|1|
|BNEP|Supported Network Packet<br>Type List|2|



Bluetooth SIG Proprietary 

Page 186 of 442 

**Assigned Numbers** / Document 

**2026-06-23** 

## **<u>6 Profiles and Services</u>** 

#### **6.1 Environmental Sensing Service** 

##### **6.1.1 Permitted Characteristics** 

The list below specifies the characteristics that are permitted for use with the Environmental Sensing Service [7]. 

- Ammonia Concentration 

- Apparent Wind Direction 

- Apparent Wind Speed 

- Barometric Pressure Trend 

- Carbon Monoxide Concentration 

- Dew Point 

- Elevation 

- Gust Factor 

- Heat Index 

- Humidity 

- Irradiance 

- Magnetic Declination 

- Magnetic Flux Density - 2D 

- Magnetic Flux Density - 3D 

- Methane Concentration 

- Nitrogen Dioxide Concentration 

- Non-Methane Volatile Organic Compounds Concentration 

- Ozone Concentration 

- Particulate Matter - PM1 Concentration 

- Particulate Matter - PM10 Concentration 

- Particulate Matter - PM2.5 Concentration 

- Pollen Concentration 

- Pressure 

- Rainfall 

- Sulfur Dioxide Concentration 

- Sulfur Hexafluoride Concentration 

- Temperature 

- True Wind Direction 

- True Wind Speed 

- UV Index 

- Wind Chill 

#### **6.2 User Data Service** 

##### **6.2.1 Permitted Characteristics** 

The list below specifies the characteristics that are permitted for use with the User Data Service [25]. 

- Activity Goal 

- Aerobic Heart Rate Lower Limit 

- Aerobic Heart Rate Upper Limit 

- Aerobic Threshold 

- Age 

- Anaerobic Heart Rate Lower Limit 

Bluetooth SIG Proprietary 

Page 187 of 442 

**Assigned Numbers** / Document 

**2026-06-23** 

- Anaerobic Heart Rate Upper Limit 

- Anaerobic Threshold 

- Caloric Intake 

- Date of Birth 

- Date of Threshold Assessment 

- Device Wearing Position 

- Email Address 

- Fat Burn Heart Rate Lower Limit 

- Fat Burn Heart Rate Upper Limit 

- First Name 

- Five Zone Heart Rate Limits 

- Four Zone Heart Rate Limits 

- Gender 

- Handedness 

- Heart Rate Max 

- Height 

- High Intensity Exercise Threshold 

- High Resolution Height 

- Hip Circumference 

- Language 

- Last Name 

- Maximum Recommended Heart Rate 

- Middle Name 

- Preferred Units 

- Resting Heart Rate 

- Sedentary Interval Notification 

- Sport Type for Aerobic and Anaerobic Thresholds 

- Stride Length 

- Three Zone Heart Rate Limits 

- Two Zone Heart Rate Limits 

- VO2 Max 

- Waist Circumference 

- Weight 

Bluetooth SIG Proprietary 

Page 188 of 442 

**Assigned Numbers** / Document 

**2026-06-23** 

#### **6.3 A/V Distribution Protocol (AVDTP)** 

##### **6.3.1 Media Type** 

###### Last Modified: 2024-02-05 

Filename: assigned_numbers/profiles_and_services/avdtp/avdtp_media_type.yaml 

|**Value**|**Description**|
|---|---|
|0x0|Audio|
|0x1|Video|
|0x2|Multimedia|



##### **6.3.2 A/V Content Protection Method** 

Last Modified: 2024-02-05 

Filename: assigned_numbers/profiles_and_services/avdtp/avdtp_content_protection_method.yaml 

|**Value**|**Mnemonic**|**Content Security**<br>**(reference)**|**Usage in Bluetooth (reference)**|
|---|---|---|---|
|0x0001|DTCP|Please see<br>www.dtcp.com for<br>details of how Digital<br>Transmission Content<br>Protection (DTCP) is<br>mapped to the<br>Bluetooth AV transport<br>services and for<br>information about<br>DTCP licensing by the<br>Digital Transmission<br>Licensing<br>Administrator (DTLA).|Bluetooth A/V Distribution<br>Transport Protocol Specification<br>”Content Protection Capabilities”,<br>Section 8.19.6|
|0x0002|SCMS-T|SCMS-T uses Cp-bit<br>and L-bit that are<br>defined by<br>IEC60958-3:1999 and<br>IEC61119-6:1992. For<br>definition of L-bit,<br>normal logic, instead<br>of reverse situation<br>defined in section<br>4.3.1 of IEC60958-3,<br>shall be applied.|The Contents Protection Header<br>(CP Header) defined by A2DP is<br>used for transmitting these two bits<br>(Cp-bit and L-bit). CP Header has<br>a one-byte length. The bit0 field of<br>CP Headers is used for the L-bit<br>and the bit 1 field of CP Header is<br>used for the Cp-bit. Other bits<br>(from bit2 to bit7) are defined as<br>the RFA field.|



General Note: The following applies to any A/V Content Protection Method identifier: 

- The Content Protection method includes a reference to either the relevant controlling entity or a description of the method. 

Bluetooth SIG Proprietary 

Page 189 of 442 

**Assigned Numbers** / Document 

**2026-06-23** 

- The Content Protection method indicates how the identifier is to be used in the context of Bluetooth A/V. 

#### **6.4 A/V Remote Control Profile (AVRCP)** 

See also Section 5.1.2. 

##### **6.4.1 Major Player Type** 

###### Last Modified: 2024-02-05 

Filename: assigned_numbers/profiles_and_services/avrcp/avrcp_major_player_type.yaml 

|**Value**|**Parameter Description**|
|---|---|
|0x01|Audio|
|0x02|Video|
|0x04|Broadcasting Audio|
|0x08|Broadcasting Video|



##### **6.4.2 Player Sub Type** 

###### Last Modified: 2024-02-05 

Filename: assigned_numbers/profiles_and_services/avrcp/avrcp_player_sub_type.yaml 

|**Value**|**Parameter Description**|
|---|---|
|0x00000001|Audio Book|
|0x00000002|Podcast|



##### **6.4.3 Folder Type** 

###### Last Modified: 2024-02-05 

Filename: assigned_numbers/profiles_and_services/avrcp/avrcp_folder_type.yaml 

|**Value**|**Parameter Description**|
|---|---|
|0x00|Mixed|
|0x01|Titles|
|0x02|Albums|
|0x03|Artists|
|0x04|Genres|
|0x05|Playlists|
|0x06|Years|



Bluetooth SIG Proprietary 

Page 190 of 442 

**Assigned Numbers** / Document 

**2026-06-23** 

##### **6.4.4 Media Type** 

###### Last Modified: 2024-02-05 

Filename: assigned_numbers/profiles_and_services/avrcp/avrcp_media_type.yaml 

|**Value**|**Parameter Description**|
|---|---|
|0x00|Audio|
|0x01|Video|



##### **6.4.5 List of Media Attributes** 

###### Last Modified: 2024-02-05 

Filename: assigned_numbers/profiles_and_services/avrcp/avrcp_media_attributes.yaml 

|**Value**|**Description**|**Allowed Values**|
|---|---|---|
|0x0|Illegal, Should Not Be Used||
|0x1|Title of the Media|Any text encoded in specific character<br>set|
|0x2|Name of the Artist|Any text encoded in specified<br>character set|
|0x3|Name of the Album|Any text encoded in specified<br>character set|
|0x4|Number of the Media (e.g., Track<br>Number in a CD)|Numeric ASCII text with zero<br>suppresses|
|0x5|Total Number of the Media (e.g., Total<br>Number of Tracks in a CD)|Numeric ASCII text with zero<br>suppresses|
|0x6|Genre|Any text encoded in specified<br>character set|
|0x7|Playing Time, in Milliseconds|Numeric ASCII text with zero<br>suppresses (ex. 2min30sec = 150000)|



##### **6.4.6 Player Application Settings** 

###### Last Modified: 2024-02-05 

Filename: assigned_numbers/profiles_and_services/avrcp/avrcp_player_application_settings.yaml 

|**Player**<br>**Application**<br>**Setting**<br>**Attribute**|**Attribute Description**|**Player Application Setting**<br>**ValueID**|**Description**|
|---|---|---|---|
|0x00|Illegal - Should Not Be<br>Used|None||
|0x01|Equalizer ON/OFF<br>Status|0x01|OFF|



Bluetooth SIG Proprietary 

Page 191 of 442 

**Assigned Numbers** / Document 

**2026-06-23** 

|||0x02|ON|
|---|---|---|---|
|0x02|Repeat Mode Status|0x01|OFF|
|||0x02|Single Track Repeat|
|||0x03|All Track Repeat|
|||0x04|Group Repeat|
|0x03|Shuffle ON/OFF<br>Status|0x01|OFF|
|||0x02|All Tracks Shuffle|
|||0x03|Group Shuffle|
|0x04|Scan ON/OFF Status|0x01|OFF|
|||0x02|All Tracks Scan|
|||0x03|Group Scan|
|0x80‐0xFF|Provided for TG<br>Driven Static Media<br>Player Menu<br>Extension by CT|||



#### **6.5 Advanced Audio Distribution Profile (A2DP)** 

See also Section 5.1.1. 

##### **6.5.1 Audio Codec ID** 

###### Last Modified: 2024-02-05 

Filename: assigned_numbers/profiles_and_services/a2dp/a2dp.yaml 

|**Value**|**Codec**|**Specified In**|**Used in**|
|---|---|---|---|
|0x00|SBC|A2DP|A2DP|
|0x01|MPEG-1, 2 Audio|A2DP|A2DP|
|0x02|MPEG-2, 4 AAC|A2DP|A2DP|
|0x03|MPEG-D USAC|A2DP|A2DP|
|0x04|ATRAC Family|A2DP|A2DP|
|0xFF|Vendor Specific (formerly<br>Non-A2DP)|A2DP|A2DP|



#### **6.6 Video Distribution Profile (VDP)** 

##### **6.6.1 Video Codec ID** 

###### Last Modified: 2024-02-05 

Filename: assigned_numbers/profiles_and_services/vdp/vdp.yaml 

Bluetooth SIG Proprietary 

Page 192 of 442 

**Assigned Numbers** / Document 

**2026-06-23** 

|**Value**|**Codec**|**Specified In**|**Used in**|
|---|---|---|---|
|0x00|H.263 Baseline|VDP|VDP|
|0x01|MPEG-4 Visual|VDP|VDP|
|0x02|H.263 Profile 3|VDP|VDP|
|0x04|H.263 Profile 8|VDP|VDP|
|0xFF|Vendor Specific (formerly<br>Non-VDP)|VDP|VDP|



Bluetooth SIG Proprietary 

Page 193 of 442 

**Assigned Numbers** / Document 

**2026-06-23** 

#### **6.7 Transport Discovery Service** 

##### **6.7.1 Organization IDs** 

Last Modified: 2024-02-05 

Filename: assigned_numbers/profiles_and_services/tds/transport_discovery_service_id.yaml 

|**Value**|**Definition**|
|---|---|
|0x01|Bluetooth SIG|
|0x02|Wi-Fi Alliance Neighbor Awareness Networking|
|0x03|Wi-Fi Alliance Service Advertisement|



Bluetooth SIG Proprietary 

Page 194 of 442 

**Assigned Numbers** / Document 

**2026-06-23** 

#### **6.8 Health Device Profile** 

See also Section 5.1.18. 

##### **6.8.1 Data Exchange Specifications** 

###### Last Modified: 2024-02-05 

Filename: assigned_numbers/profiles_and_services/hdp/data_exchange_specs.yaml 

|**Value**|**Document Name**|**Document Number**|
|---|---|---|
|0x01|Health informatics -<br>Personal health device<br>communication -<br>Application profile -<br>Optimized exchange<br>protocol|ISO/IEEE 11073-20601|



##### **6.8.2 Device Data Specializations** 

###### Last Modified: 2024-02-05 

Filename: assigned_numbers/profiles_and_services/hdp/device_data_specs.yaml 

|**MDEP Data**<br>**Type (IEEE**<br>**11073-**<br>**10101**<br>**Nomencla-**<br>**ture Data**<br>**Type Code)**|**Data Type**|**IEEE 11073**<br>**Document**<br>**Number**|**IEEE 11073 Document Name**|**Notes**|
|---|---|---|---|---|
|0x1000|Hydra|11073-20601|IEEE 11073 Part 10101:<br>Nomenclature; IEEE 11073-20601:<br>Application profile - Optimized<br>Exchange Protocol||
|0x1004|Pulse Oximeter|11073-10404|Health informatics - Personal health<br>device communication - Device<br>specialization - Pulse oximeter||
|0x1006|Basic ECG<br>(heart rate)|11073-10406|Health informatics - Personal health<br>device communication - Device<br>specialization - Basic ECG (heart rate)||
|0x1007|Blood Pressure<br>Monitor|11073-10407|Health informatics - Personal health<br>device communication - Device<br>specialization - Blood pressure<br>monitor||
|0x1008|Body<br>Thermometer|11073-10408|Health informatics - Personal health<br>device communication - Device<br>specialization - Body thermometer||



Bluetooth SIG Proprietary 

Page 195 of 442 

**Assigned Numbers** / Document 

**2026-06-23** 

|0x100F|Body Weight<br>Scale|11073-10415|Health informatics - Personal health<br>device communication - Device<br>specialization - Body weight scale||
|---|---|---|---|---|
|0x1011|Glucose Meter|11073-10417|Health informatics - Personal health<br>device communication - Device<br>Specialization - Glucose meter||
|0x1012|International<br>Normalized<br>Ratio (INR)<br>Monitor|11073-10418|Health informatics - Personal health<br>device communication - Device<br>Specialization - International<br>Normalized Ratio (INR) Monitor||
|0x1014|Body<br>Composition<br>Analyzer|11073-10420|Health informatics - Personal health<br>device communication - Device<br>specialization - Body composition<br>analyzer||
|0x1015|Peak Flow<br>Monitor|11073-10421|Health informatics - Personal health<br>device communication - Device<br>specialization - Peak flow monitor||
|0x101C|Power Status<br>Monitor|11073-10427|Health infomatics - Personal health<br>device communication - Part 10427:<br>Device specialization - Power Status<br>Monitor of Personal Health Devices||
|0x1029|Cardiovascular<br>Fitness and<br>Activity Monitor|11073-10441|Health informatics - Personal health<br>device communication - Device<br>specialization - Cardiovascular Fitness<br>and Activity Monitor|Note 1|
|0x102A|Strength<br>Fitness<br>Equipment|11073-10442|Health informatics - Personal health<br>device communication - Device<br>specialization - Strength fitness<br>equipment|Note 1|
|0x1047|Independent<br>Living Activity<br>Hub|11073-10471|Health informatics - Personal health<br>device communication - Device<br>specialization - Independent Living<br>Activity Hub|Note 1|
|0x1048|Medication<br>monitor|11073-10472|Health informatics - Personal health<br>device communication - Device<br>specialization - Medication monitor||
|0x1049|Generic|11073-20601|IEEE 11073 Part 10101:<br>Nomenclature; IEEE 11073-20601:<br>Application profile - Optimized<br>Exchange Protocol||
|0x1068|Step Counter<br>based on<br>10441|11073-10441|Health informatics - Personal health<br>device communication - Device<br>specialization - Step Counter based<br>on 10441|Note 2|
|0x1075|Fall Sensor|11073-10471|Health informatics - Personal health<br>device communication - Device<br>specialization - Fall Sensor|Note 3|



Bluetooth SIG Proprietary 

Page 196 of 442 

**Assigned Numbers** / Document 

**2026-06-23** 

|0x1076|Personal<br>Emergency<br>Response<br>Sensor|11073-10471|Health informatics - Personal health<br>device communication - Device<br>specialization - Personal Emergency<br>Response Sensor|Note 3|
|---|---|---|---|---|
|0x1077|Smoke Sensor|11073-10471|Health informatics - Personal health<br>device communication - Device<br>specialization - Smoke Sensor|Note 3|
|0x1078|Carbon<br>Monoxide (CO)<br>Sensor|11073-10471|Health informatics - Personal health<br>device communication - Device<br>specialization - Carbon Monoxide<br>(CO) Sensor|Note 3|
|0x1079|Water Sensor|11073-10471|Health informatics - Personal health<br>device communication - Device<br>specialization - WaterSensor|Note 3|
|0x107A|Gas Sensor|11073-10471|Health informatics - Personal health<br>device communication - Device<br>specialization - Gas Sensor|Note 3|
|0x107B|Motion Sensor|11073-10471|Health informatics - Personal health<br>device communication - Device<br>specialization - Motion Sensor|Note 3|
|0x107C|Property Exit<br>Sensor|11073-10471|Health informatics - Personal health<br>device communication - Device<br>specialization - Property Exit Sensor|Note 3|
|0x107D|Enuresis<br>Sensor|11073-10471|Health informatics - Personal health<br>device communication - Device<br>specialization - Enuresis Sensor|Note 3|
|0x107E|Contact<br>Closure Sensor|11073-10471|Health informatics - Personal health<br>device communication - Device<br>specialization - Contact Closure<br>Sensor|Note 3|
|0x107F|Usage Sensor|11073-10471|Health informatics - Personal health<br>device communication - Device<br>specialization - Usage Sensor|Note 3|
|0x1080|Switch Sensor|11073-10471|Health informatics - Personal health<br>device communication - Device<br>specialization - Switch Sensor|Note 3|
|0x1081|Medication<br>Dosing Sensor|11073-10471|Health informatics - Personal health<br>device communication - Device<br>specialization - Medication Dosing<br>Sensor|Note 3|
|0x1082|Temperature<br>Sensor|11073-10471|Health informatics - Personal health<br>device communication - Device<br>specialization - Temperature Sensor|Note 3|



**Note 1** : Only for devices that support all objects. **Note 2** : Profiling on top of IEEE 11073-10415 is part of Continua Health Allicance Interoperability Guideline v1.5 or later. 

Bluetooth SIG Proprietary 

Page 197 of 442 

**Assigned Numbers** / Document 

**2026-06-23** 

**Note 3** : Profiling on top of IEEE 11073-10471 is part of Continua Health Allicance Interoperability Guideline v1.5 or later. 

Bluetooth SIG Proprietary 

Page 198 of 442 

**Assigned Numbers** / Document 

**2026-06-23** 

#### **6.9 Message Access Profile** 

See also Section 5.1.21. 

##### **6.9.1 Presence** 

###### Last Modified: 2024-02-05 

Filename: assigned_numbers/profiles_and_services/map/presence.yaml 

|**Value**|**Parameter Name**|
|---|---|
|0x00|Unknown|
|0x01|Offline|
|0x02|Online|
|0x03|Away|
|0x04|Do Not Disturb|
|0x05|Busy|
|0x06|In a Meeting|



##### **6.9.2 Chat State** 

###### Last Modified: 2024-02-05 

Filename: assigned_numbers/profiles_and_services/map/chat_state.yaml 

|**Value**|**Parameter Name**|
|---|---|
|0x00|Unknown|
|0x01|Inactive|
|0x02|Active|
|0x03|Composing|
|0x04|Paused Composing|
|0x05|Gone|



##### **6.9.3 Message Extended Data** 

###### Last Modified: 2024-02-05 

Filename: assigned_numbers/profiles_and_services/map/message_extended_data.yaml 

|**Value**|**Parameter Name**|**Format**|
|---|---|---|
|0x00|Number of Facebook Likes|Integer|
|0x01|Number of Twitter Followers|Integer|
|0x02|Number of Twitter Retweets|Integer|



Bluetooth SIG Proprietary 

Page 199 of 442 

**Assigned Numbers** / Document 

**2026-06-23** 

|0x03<br>Number of Google +1s<br>Integer|
|---|



Bluetooth SIG Proprietary 

Page 200 of 442 

**Assigned Numbers** / Document 

**2026-06-23** 

#### **6.10 Hands-Free Profile** 

See also Section 5.1.15. 

##### **6.10.1 HF Indicators** 

###### Last Modified: 2024-02-05 

Filename: assigned_numbers/profiles_and_services/hfp/hf_indicators.yaml 

|**Value**|**Description**|
|---|---|
|0x01|0: Enhanced Safety is Disabled|
||1: Enhanced Safety is Enabled|
|0x02|0 - 100: Remaining level of Battery|



##### **6.10.2 Bearer Technology** 

###### Last Modified: 2024-02-05 

Filename: assigned_numbers/profiles_and_services/hfp/bearer_technology.yaml 

|**Value**|**Technology**|
|---|---|
|0x01|3G|
|0x02|4G|
|0x03|LTE|
|0x04|Wi-Fi|
|0x05|5G|
|0x06|GSM|
|0x07|CDMA|
|0x08|2G|
|0x09|WCDMA|



##### **6.10.3 Uniform Caller Identifiers** 

See Section 6.11.1. 

Bluetooth SIG Proprietary 

Page 201 of 442 

**Assigned Numbers** / Document 

**2026-06-23** 

#### **6.11 Telephone Bearer Service** 

##### **6.11.1 Uniform Caller Identifiers** 

Last Modified: 2024-02-05 

Filename: assigned_numbers/profiles_and_services/tbs/uniform_caller_identifiers.yaml 

|**Client ID**|**Client Name**|**Company Name**|**Notes**|
|---|---|---|---|
|E.164|Any built-in dialer able to<br>make standard phone<br>calls|N/A|For example, mobile service<br>provider or landline|
|bbm|Blackberry Messenger|Blackberry||
|bbv|Blackberry Voice|Blackberry||
|chsec|ChatSecure||https://chatsecure.org/about/|
|con|ChatOn|Samsung||
|eyebm|Eyebeam|Counterpath||
|fbch|Facebook Chat|Meta||
|ftime|Facetime|Apple||
|gtalk|Google Talk|Google LLC||
|hgus|Hangouts|Google LLC||
|icht|iChat|Apple||
|imo|imo.im|PageBites, Inc.|https://imo.im/register|
|jabr|Jabber|Cisco||
|kik|KIK|KIK Interactive||
|kkt|KakaoTalk|Daum Communications||
|lbn|Libon|Orange Vallée||
|lne|Line|Line Corporation|https://line.me/en/|
|lync|Lync|Microsoft||
|mme|MessageMe|Yahoo||
|mngr|Messanger|Meta||
|nbuz|Nimbuzz|Nimbuzz BV / Nimbuzz<br>Internet India Pvt. Ltd||
|ov|ooVo|ooVoo LLC||
|qik|Skye Qik|Microsoft||
|qq|QQ|Tencent Technology||
|rnds|Rounds|Rounds Entertainment<br>Ltd.||
|skype|Skype|Microsoft||
|spika|Spika|Studio Djetelina d.o.o.||



Bluetooth SIG Proprietary 

Page 202 of 442 

**Assigned Numbers** / Document 

**2026-06-23** 

|tgo|Tango|TangoMe Inc.||
|---|---|---|---|
|tgrm|Telegram|Telegram Messenger LLP||
|un000 -<br>un999|Unknown|Unknown|If the client is unknown, the host<br>OS should assign a string from<br>the range un000 to un999<br>inclusive. This string should be<br>assigned to this client for the<br>time it was installed to when it is<br>uninstalled, after which the<br>string can be reused.|
|vbr|Viber|Rakuten||
|vonm|Vonage Mobile|Vonage||
|wbrtc|Any WebRTC Enabled<br>Browser|N/A|Firefox and Chrome Browsers|
|wcht|WeChat|Tencent||
|wtsap|WhatsApp|Meta||



Bluetooth SIG Proprietary 

Page 203 of 442 

**Assigned Numbers** / Document 

**2026-06-23** 

#### **6.12 Generic Audio** 

##### **6.12.1 Audio Location Definitions** 

###### Last Modified: 2024-02-05 

Filename: assigned_numbers/profiles_and_services/generic_audio/audio_location_definitions.yaml 

|**Value**|**Audio Location**|
|---|---|
|0x00000000|Mono Audio (no specified Audio Location)|
|0x00000001|Front Left|
|0x00000002|Front Right|
|0x00000004|Front Center|
|0x00000008|Low Frequency Effects 1|
|0x00000010|Back Left|
|0x00000020|Back Right|
|0x00000040|Front Left of Center|
|0x00000080|Front Right of Center|
|0x00000100|Back Center|
|0x00000200|Low Frequency Effects 2|
|0x00000400|Side Left|
|0x00000800|Side Right|
|0x00001000|Top Front Left|
|0x00002000|Top Front Right|
|0x00004000|Top Front Center|
|0x00008000|Top Center|
|0x00010000|Top Back Left|
|0x00020000|Top Back Right|
|0x00040000|Top Side Left|
|0x00080000|Top Side Right|
|0x00100000|Top Back Center|
|0x00200000|Bottom Front Center|
|0x00400000|Bottom Front Left|
|0x00800000|Bottom Front Right|
|0x01000000|Front Left Wide|
|0x02000000|Front Right Wide|
|0x04000000|Left Surround|
|0x08000000|Right Surround|



Bluetooth SIG Proprietary 

Page 204 of 442 

**Assigned Numbers** / Document 

**2026-06-23** 

##### **6.12.2 Audio Input Type Definitions** 

###### Last Modified: 2026-05-29 

Filename: assigned_numbers/profiles_and_services/generic_audio/audio_input_type_definitions.yaml 

|**Value**|**Label**|**Description**|
|---|---|---|
|0x00|Unspecified|Unspecified Input|
|0x01|Bluetooth|Bluetooth Audio Stream|
|0x02|Microphone|Microphone|
|0x03|Analog|Analog Interface|
|0x04|Digital|Digital Interface|
|0x05|Radio|AM/FM/XM/etc.|
|0x06|Streaming|Streaming Audio Source|
|0x07|Ambient|Transparency/Pass-through|
|0x08|T-Coil|Tele-Coil Audio Input|



##### **6.12.3 Context Type** 

###### Last Modified: 2025-04-23 

Filename: assigned_numbers/profiles_and_services/generic_audio/context_type.yaml 

|**Value**|**Label**|**Description**|
|---|---|---|
|0x0001|Unspecified|Identifies audio where the use case context does not<br>match any other defined value, or where the context is<br>unknown or cannot be determined.|
|0x0002|Conversational|Conversation between humans, for example, in telephony<br>or video calls, including traditional cellular as well as VoIP<br>and Push-to-Talk|
|0x0004|Media|Media, for example, music playback, radio, podcast or<br>movie soundtrack, or tv audio|
|0x0008|Game|Audio associated with video gaming, for example gaming<br>media; gaming effects; music and in-game voice chat<br>between participants; or a mix of all the above|
|0x0010|Instructional|Instructional audio, for example, in navigation,<br>announcements, or user guidance|
|0x0020|Voice Assistants|Man-machine communication, for example, with voice<br>recognition or virtual assistants|
|0x0040|Live|Live audio, for example, from a microphone where audio<br>is perceived both through a direct acoustic path and<br>through an LE Audio Stream|
|0x0080|Sound Effects|Sound effects including keyboard and touch feedback;<br>menu and user interface sounds; and other system<br>sounds|



Bluetooth SIG Proprietary 

Page 205 of 442 

**Assigned Numbers** / Document 

**2026-06-23** 

|0x0100|Notifications|Notification and reminder sounds; attention-seeking<br>audio, for example, in beeps signaling the arrival of a<br>message|
|---|---|---|
|0x0200|Ringtone|Alerts the user to an incoming call, for example, an<br>incoming telephony or video call, including traditional<br>cellular as well as VoIP and Push-to-Talk|
|0x0400|Alerts|Alarms and timers; immediate alerts, for example, in a<br>critical battery alarm, timer expiry or alarm clock, toaster,<br>cooker, kettle, microwave, etc.|
|0x0800|Emergency Alarm|Emergency alarm Emergency sounds, for example, fire<br>alarms or other urgent alerts|



##### **6.12.4 Codec_Specific_Capabilities LTV Structures** 

###### **6.12.4.1 Supported_Sampling_Frequencies** 

###### Last Modified: 2024-02-05 

Filename: assigned_numbers/profiles_and_services/generic_audio/codec_capabilities/supported_sampling_frequencies.yaml 

|**Parameter**|**Size (Octets)**<br>**Value**|**Description**|
|---|---|---|
|Length|1<br>0x03||
|Type|1<br>0x01||
|Value|2<br>Bit 0: 8000 Hz|Bitfield|
||Bit 1: 11025 Hz|0b1 = supported|
||Bit 2: 16000 Hz|0b0 = not supported|
||Bit 3: 22050 Hz||
||Bit 4: 24000 Hz||
||Bit 5: 32000 Hz||
||Bit 6: 44100 Hz||
||Bit 7: 48000 Hz||
||Bit 8: 88200 Hz||
||Bit 9: 96000 Hz||
||Bit 10: 176400 Hz||
||Bit 11: 192000 Hz||
||Bit 12: 384000 Hz||



###### **6.12.4.2 Supported_Frame_Durations** 

Last Modified: 2024-08-12 

Bluetooth SIG Proprietary 

Page 206 of 442 

**Assigned Numbers** / Document 

**2026-06-23** 

Filename: assigned_numbers/profiles_and_services/generic_audio/codec_capabilities/supported_frame_durations.yaml 

|**Parameter**|**Size (Octets)**|**Value**|**Description**|
|---|---|---|---|
|Length|1|0x02||
|Type|1|0x02||
|Value|1|Bit 0: 7.5 ms frame duration|0b1 = supported, 0b0 = not supported|
|||Bit 1: 10 ms frame duration|0b1 = supported, 0b0 = not supported|
|||Bit 4: 7.5 ms preferred|Valid only when 7.5 ms is supported<br>and 10 ms is supported.|
|||Bit 5: 10 ms preferred|Valid only when 7.5 ms is supported<br>and 10 ms is supported.|



Note: The combination of 0b11 is not valid for bit 4 and 5. 

###### **6.12.4.3 Supported_Audio_Channel_Counts** 

###### Last Modified: 2024-02-05 

Filename: assigned_numbers/profiles_and_services/generic_audio/codec_capabilities/supported_audio_channel_counts.yaml 

|**Parameter**|**Size (Octets)**|**Value**|**Description**|
|---|---|---|---|
|Length|1|0x02||
|Type|1|0x03||
|value|1|Bit 0: Channel count: 1|Bitfield|
|||Bit 1: Channel count: 2|0b0 = Channel count not supported|
|||Bit 2: Channel count: 3|0b1 = Channel count supported|
|||Bit 3: Channel count: 4||
|||Bit 4: Channel count: 5||
|||Bit 5: Channel count: 6||
|||Bit 6: Channel count: 7||
|||Bit 7: Channel count: 8||



###### **6.12.4.4 Supported_Octets_Per_Codec_Frame** 

###### Last Modified: 2024-02-05 

Filename: assigned_numbers/profiles_and_services/generic_audio/codec_capabilities/supported_octets_per_codec_frame.yaml 

|**Parameter**|**Size (Octets)**|**Value**|
|---|---|---|
|Length|1|0x05|
|Type|1|0x04|



Bluetooth SIG Proprietary 

Page 207 of 442 

**Assigned Numbers** / Document 

**2026-06-23** 

|Value<br>4|Octet 0-1: Minimum number of octets supported<br>per codec frame|
|---|---|
||Octet 2-3: Maximum number of octets supported<br>per codec frame|



###### **6.12.4.5 Supported_Max_Codec_Frames_Per_SDU** 

###### Last Modified: 2024-02-05 

Filename: assigned_numbers/profiles_and_services/generic_audio/codec_capabilities/supported_max_codec_frames_per_sdu.yaml 

|**Parameter**|**Size (Octets)**|**Value**|**Description**|
|---|---|---|---|
|Length|1|0x02||
|Type|1|0x05||
|Value|1||Maximum number of codec frames per<br>SDU supported by this device|



##### **6.12.5 Codec_Specific_Configuration LTV structures** 

###### **6.12.5.1 Sampling_Frequency** 

Last Modified: 2024-02-05 

Filename: assigned_numbers/profiles_and_services/generic_audio/codec_configuration_ltv/sampling_frequency_configuration.yaml 

|**Parameter**<br>**Size (Octets)**|**Value**|**Description**|
|---|---|---|
|Length<br>1|0x02||
|Type<br>1|0x01||
|Value<br>1|0x01: 8000 Hz|Selected codec sampling frequency|
||0x02: 11025 Hz||
||0x03: 16000 Hz||
||0x04: 22050 Hz||
||0x05: 24000 Hz||
||0x06: 32000 Hz||
||0x07: 44100 Hz||
||0x08: 48000 Hz||
||0x09: 88200 Hz||
||0x0A: 96000 Hz||
||0x0B: 176400 Hz||
||0x0C: 192000 Hz||



Bluetooth SIG Proprietary 

Page 208 of 442 

**Assigned Numbers** / Document 

**2026-06-23** 

0x0D: 384000 Hz 

###### **6.12.5.2 Frame_Duration** 

###### Last Modified: 2024-02-05 

Filename: assigned_numbers/profiles_and_services/generic_audio/codec_configuration_ltv/frame_duration_configuration.yaml 

|**Parameter**|**Size (Octets)**|**Value**|**Description**|
|---|---|---|---|
|Length|1|0x02||
|Type|1|0x02||
|Value|1|0x00: Use 7.5 ms codec<br>frames|Selected codec frame duration|
|||0x01: Use 10 ms codec<br>frames||



###### **6.12.5.3 Audio_Channel_Allocation** 

###### Last Modified: 2024-02-05 

Filename: assigned_numbers/profiles_and_services/generic_audio/codec_configuration_ltv/audio_channel_allocation_configuration.yaml 

|**Parameter**|**Size (Octets)**|**Value**|**Description**|
|---|---|---|---|
|Length|1|0x05||
|Type|1|0x03||
|Value|4||4-octet bitfield of Audio Location<br>values|



###### **6.12.5.4 Octets_Per_Codec_Frame** 

###### Last Modified: 2024-02-05 

Filename: assigned_numbers/profiles_and_services/generic_audio/codec_configuration_ltv/octets_per_codec_frame_configuration.yaml 

|**Parameter**|**Size (Octets)**|**Value**|**Description**|
|---|---|---|---|
|Length|1|0x03||
|Type|1|0x04||
|Value|2||Number of octets used per codec<br>frame|



###### **6.12.5.5 Codec_Frame_Blocks_Per_SDU** 

Bluetooth SIG Proprietary 

Page 209 of 442 

**Assigned Numbers** / Document 

**2026-06-23** 

###### Last Modified: 2024-02-05 

Filename: assigned_numbers/profiles_and_services/generic_audio/codec_configuration_ltv/codec_frame_blocks_per_sdu_configuration.yaml 

|**Parameter**|**Size (Octets)**|**Value**|**Description**|
|---|---|---|---|
|Length|1|0x02||
|Type|1|0x05||
|Value|1||Number of blocks of codec frames per<br>SDU|



##### **6.12.6 Metadata LTV structures** 

###### **6.12.6.1 Preferred_Audio_Contexts** 

###### Last Modified: 2024-02-05 

Filename: assigned_numbers/profiles_and_services/generic_audio/metadata_ltv/preferred_audio_contexts.yaml 

|**Parameter**|**Size (Octets)**|**Value**|**Description**|
|---|---|---|---|
|Length|1|0x03||
|Type|1|0x01||
|Value|2||Bitfield of Context Type values.<br>See Context Type values defined in<br>Section 6.12.3.<br>0b0 = Context Type is not a preferred<br>use case for this codec configuration.<br>0b1 = Context Type is a preferred use<br>case for this codec configuration.|



###### **6.12.6.2 Streaming_Audio_Contexts** 

###### Last Modified: 2024-02-05 

Filename: assigned_numbers/profiles_and_services/generic_audio/metadata_ltv/streaming_audio_contexts.yaml 

|**Parameter**|**Size (Octets)**|**Value**|**Description**|
|---|---|---|---|
|Length|1|0x03||
|Type|1|0x02||



Bluetooth SIG Proprietary 

Page 210 of 442 

**Assigned Numbers** / Document 

**2026-06-23** 

|Value<br>2|Bitfield of Context Type values|
|---|---|
||See Context Type values defined in<br>Section 6.12.3.|
||0b0 = Context Type is not an intended<br>use case for this Audio Stream.<br>0b1 = Context Type is an intended use<br>case for this Audio Stream.|



###### **6.12.6.3 Program_Info** 

###### Last Modified: 2024-02-05 

Filename: assigned_numbers/profiles_and_services/generic_audio/metadata_ltv/program_info.yaml 

|**Parameter**|**Size (Octets)**|**Value**|**Description**|
|---|---|---|---|
|Length|1|Varies||
|Type|1|0x03||
|Value|Varies||Title and/or summary of Audio Stream<br>content: UTF-8 format|



###### **6.12.6.4 Language** 

###### Last Modified: 2024-02-05 

Filename: assigned_numbers/profiles_and_services/generic_audio/metadata_ltv/language.yaml 

|**Parameter**|**Size (Octets)**|**Value**|**Description**|
|---|---|---|---|
|Length|1|0x04||
|Type|1|0x04||
|Value|3||3-byte, lower case language code as<br>defined in ISO 639-3|



###### **6.12.6.5 CCID_List** 

###### Last Modified: 2024-02-05 

Filename: assigned_numbers/profiles_and_services/generic_audio/metadata_ltv/ccid_list.yaml 

|**Parameter**|**Size (Octets)**|**Value**|**Description**|
|---|---|---|---|
|Length|1|Varies||
|Type|1|0x05||
|Value|Varies||Array of CCID values|



Bluetooth SIG Proprietary 

Page 211 of 442 

**Assigned Numbers** / Document 

**2026-06-23** 

###### **6.12.6.6 Parental_Rating** 

Last Modified: 2024-02-05 

Filename: assigned_numbers/profiles_and_services/generic_audio/metadata_ltv/parental_rating.yaml 

|**Parameter**|**Size (Octets)**|**Value**|**Description**|
|---|---|---|---|
|Length|1|0x02||
|Type|1|0x06||
|Value|1|0x00: no rating|Bits 0-3 Value representing the<br>parental rating.|
|||0x01: recommended for<br>listeners of any age||
|||Other values: recommended<br>for listeners of age Y years,<br>where Y = value + 3 years.<br>e.g., 0x05 = recommended<br>for listeners of 8 years or<br>older.|The numbering scheme aligns with<br>Annex F of EN 300 707 v1.2.1 which<br>defines parental rating for viewing.<br>https://www.etsi.org<br>ETSI EN 300 707 V1.2.1 (2002-12)|



###### **6.12.6.7 Program_Info_URI** 

###### Last Modified: 2024-02-05 

Filename: assigned_numbers/profiles_and_services/generic_audio/metadata_ltv/programinfo_uri.yaml 

|**Parameter**|**Size (Octets)**|**Value**|**Description**|
|---|---|---|---|
|Length|1|Varies||
|Type|1|0x07||
|Value|Varies||A UTF-8 formatted URL link used to<br>present more information about<br>Program_Info|



###### **6.12.6.8 Extended_Metadata** 

Last Modified: 2024-02-05 

Filename: assigned_numbers/profiles_and_services/generic_audio/metadata_ltv/extended_metadata.yaml 

|**Parameter**|**Size (Octets)**|**Value**|
|---|---|---|
|Length|1|Varies|
|Type|1|0xFE|
|Value|Varies|Octet 0–1: Extended Metadata Type|
|||Octet 2–254: Extended Metadata|



Bluetooth SIG Proprietary 

Page 212 of 442 

**Assigned Numbers** / Document 

**2026-06-23** 

###### **6.12.6.9 Vendor_Specific** 

###### Last Modified: 2024-02-05 

Filename: assigned_numbers/profiles_and_services/generic_audio/metadata_ltv/vendor_specific.yaml 

|**Parameter**<br>**Size (Octets)**|**Value**|**Description**|
|---|---|---|
|Length<br>1|Varies||
|Type<br>1|0xFF||
|Value<br>Varies|Octet 0–1: Company_ID|Company ID values are defined in<br>Bluetooth Assigned Numbers|
|**6.12.6.10**<br>**Audio_Active_S**<br>Last Modified: 2024-02-05|Octet 2–254:<br>Vendor-Specific Metadata<br>**tate**||
|Filename: assigned_numbers/p<br>**Parameter**<br>**Si**|rofiles_and_services/generic<br>**ze (Octets)**<br>**Value**|_audio/metadata_ltv/audio_active_state.y|
|Length|1<br>0x02||
|Type|1<br>0x08||
|Value|1<br>0x00: No au|dio data is being transmitted|
||0x01: Audio|data is being transmitted|



###### **6.12.6.10 Audio_Active_State** 

###### Last Modified: 2024-02-05 

Filename: assigned_numbers/profiles_and_services/generic_audio/metadata_ltv/audio_active_state.yaml 

###### **6.12.6.11 Broadcast_Audio_Immediate_Rendering_Flag** 

###### Last Modified: 2024-02-14 

Filename: assigned_numbers/profiles_and_services/generic_audio/metadata_ltv/broadcast_audio_immediate_rendering_flag.yaml 

|**Parameter**|**Size (Octets)**|**Value**|**Description**|
|---|---|---|---|
|Length|1|0x01||
|Type|1|0x09||
|Value|0|None|Zero length|



###### **6.12.6.12 Assisted_Listening_Stream** 

###### Last Modified: 2024-02-05 

Filename: assigned_numbers/profiles_and_services/generic_audio/metadata_ltv/assisted_listening_stream.yaml 

|**Parameter**|**Size (Octets)**|**Value**|**Description**|
|---|---|---|---|



Bluetooth SIG Proprietary 

Page 213 of 442 

**Assigned Numbers** / Document 

**2026-06-23** 

|Length|1|0x02||
|---|---|---|---|
|Type|1|0x0A||
|Value|1||1 octet enumeration of Assisted<br>Listening Stream method|
||||0x00 – unspecified audio<br>enhancement|
||||All other values – RFU|



###### **6.12.6.13 Broadcast_Name** 

Last Modified: 2024-02-05 

Filename: assigned_numbers/profiles_and_services/generic_audio/metadata_ltv/broadcast_name.yaml 

|**Parameter**|**Size (Octets)**|**Value**|**Description**|
|---|---|---|---|
|Length|1|Varies||
|Type|1|0x0B||
|Value|Varies (4 – 128)||The UTF-8 string of the<br>Broadcast_Name AD Type|



Bluetooth SIG Proprietary 

Page 214 of 442 

**Assigned Numbers** / Document 

**2026-06-23** 

#### **6.13 Electronic Shelf Label Service** 

##### **6.13.1 Display Types** 

###### Last Modified: 2024-02-05 

Filename: assigned_numbers/profiles_and_services/esl/display_types.yaml 

|**Value**|**Description**|
|---|---|
|0x01|black white|
|0x02|three gray scale|
|0x03|four gray scale|
|0x04|eight gray scale|
|0x05|sixteen gray scale|
|0x06|red black white|
|0x07|yellow black white|
|0x08|red yellow black white|
|0x09|seven color|
|0x0A|sixteen color|
|0x0B|full RGB|



Bluetooth SIG Proprietary 

Page 215 of 442 

**Assigned Numbers** / Document 

**2026-06-23** 

#### **6.14 Industrial Measurement Device Service** 

##### **6.14.1 Permitted Characteristics** 

The list below specifies the characteristics that are permitted for use with the Industrial Measurement Device Service [ **IMDS** ]. 

- Acceleration 

- Force 

- Length 

- Linear Position 

- Rotational Speed 

- Temperature 

- Torque 

Bluetooth SIG Proprietary 

Page 216 of 442 

**Assigned Numbers** / Document 

**2026-06-23** 

#### **6.15 Cookware Service** 

##### **6.15.1 Permitted Characteristics** 

The list below specifies the list of characteristics, UUIDs of which are permitted to be used in Cooking Sensor Info descriptor of the Sensor Data characteristic of the Cookware Service [ **CWS** ]. 

- Cooking Temperature 

- Humidity 

- Pressure 

Bluetooth SIG Proprietary 

Page 217 of 442 

**Assigned Numbers** / Document 

**2026-06-23** 

## **7 Com an Identifiers** **<u>p y</u>** 

Company identifiers are unique numbers assigned by the Bluetooth SIG to member companies requesting one. 

To request a new Company Identifier, please submit a ticket to Bluetooth Support and select the Assigned Numbers category (login required). For those not familiar with the assignment process, please refer to Section 2.3 of the Assigned Numbers Process Document which is available on the Templates and Documents page. 

Please allow five business days for your request to be fulfilled, and another five business days from the time your request is fulfilled to view your Company Identifier on this page. 

Referenced from the following: 

- Bluetooth Core Specification [Vol 4] Part E, Section 7.1.45 [4]. 

- Bluetooth Core Specification [Vol 4] Part E, Section 7.4.1 [4]. 

- Bluetooth Core Specification [Vol 4] Part E, Section 7.4.8 [4]. 

- Bluetooth Core Specification [Vol 4] Part E, Section 7.4.10 [4]. 

- Bluetooth Core Specification [Vol 4] Part E, Section 7.7.12 [4]. 

- Bluetooth Core Specification [Vol 4] Part E, Section 7.8.109 [4]. 

- Bluetooth Core Specification [Vol 6] Part B, Section 2.4.2.13 [4]. 

- Supplement to the Bluetooth Core Specification Part A, Section 1.4.1 [22]. 

#### **7.1 Company Identifiers by Value** 

Last Modified: 2026-06-23 

Filename: assigned_numbers/company_identifiers/company_identifiers.yaml 

|**Value**|**Name**|
|---|---|
|0x0000|Ericsson AB|
|0x0001|Nokia Mobile Phones|
|0x0002|Intel Corp.|
|0x0003|IBM Corp.|
|0x0004|Toshiba Corp.|
|0x0005|3Com|
|0x0006|Microsoft|
|0x0007|Lucent|
|0x0008|Motorola|
|0x0009|Infineon Technologies AG|
|0x000A|Qualcomm Technologies International, Ltd. (QTIL)|
|0x000B|Silicon Wave|
|0x000C|Digianswer A/S|
|0x000D|Texas Instruments Inc.|
|0x000E|Parthus Technologies Inc.|
|0x000F|Broadcom Corporation|



Bluetooth SIG Proprietary 

Page 218 of 442 

**Assigned Numbers** / Document 

**2026-06-23** 

|0x0010|Mitel Semiconductor|
|---|---|
|0x0011|Widcomm, Inc.|
|0x0012|Zeevo, Inc.|
|0x0013|Atmel Corporation|
|0x0014|Mitsubishi Electric Corporation|
|0x0015|RTX A/S|
|0x0016|KC Technology Inc.|
|0x0017|Newlogic|
|0x0018|Transilica, Inc.|
|0x0019|Rohde & Schwarz GmbH & Co. KG|
|0x001A|TTPCom Limited|
|0x001B|Signia Technologies, Inc.|
|0x001C|Conexant Systems Inc.|
|0x001D|Qualcomm|
|0x001E|Inventel|
|0x001F|AVM Berlin|
|0x0020|BandSpeed, Inc.|
|0x0021|Mansella Ltd|
|0x0022|NEC Corporation|
|0x0023|WavePlus Technology Co., Ltd.|
|0x0024|Alcatel|
|0x0025|NXP B.V.|
|0x0026|C Technologies|
|0x0027|Open Interface|
|0x0028|R F Micro Devices|
|0x0029|Hitachi Ltd|
|0x002A|Symbol Technologies, Inc.|
|0x002B|Tenovis|
|0x002C|Macronix International Co. Ltd.|
|0x002D|GCT Semiconductor|
|0x002E|Norwood Systems|
|0x002F|MewTel Technology Inc.|
|0x0030|ST Microelectronics|
|0x0031|Synopsys, Inc.|
|0x0032|Red-M (Communications) Ltd|
|0x0033|Commil Ltd|



Bluetooth SIG Proprietary 

Page 219 of 442 

**Assigned Numbers** / Document 

**2026-06-23** 

|0x0034|Computer Access Technology Corporation (CATC)|
|---|---|
|0x0036|Renesas Electronics Corporation|
|0x0037|Mobilian Corporation|
|0x0038|Syntronix Corporation|
|0x0039|Integrated System Solution Corp.|
|0x003A|Panasonic Holdings Corporation|
|0x003B|Gennum Corporation|
|0x003C|BlackBerry Limited|
|0x003D|IPextreme, Inc.|
|0x003E|Systems and Chips, Inc|
|0x003F|Bluetooth SIG, Inc|
|0x0040|Seiko Epson Corporation|
|0x0041|Integrated Silicon Solution Taiwan, Inc.|
|0x0042|CONWISE Technology Corporation Ltd|
|0x0043|PARROT AUTOMOTIVE SAS|
|0x0044|Socket Mobile|
|0x0045|Atheros Communications, Inc.|
|0x0046|MediaTek, Inc.|
|0x0047|Bluegiga|
|0x0048|Marvell Technology Group Ltd.|
|0x0049|3DSP Corporation|
|0x004A|Accel Semiconductor Ltd.|
|0x004B|AUMOVIO Systems, Inc.|
|0x004C|Apple, Inc.|
|0x004D|Staccato Communications, Inc.|
|0x004E|Avago Technologies|
|0x004F|APT Ltd.|
|0x0050|SiRF Technology, Inc.|
|0x0051|Tzero Technologies, Inc.|
|0x0052|J&M Corporation|
|0x0053|Free2move AB|
|0x0054|3DiJoy Corporation|
|0x0055|Plantronics, Inc.|
|0x0056|Sony Ericsson Mobile Communications|
|0x0057|Harman International Industries, Inc.|
|0x0058|Vizio, Inc.|



Bluetooth SIG Proprietary 

Page 220 of 442 

**Assigned Numbers** / Document 

**2026-06-23** 

|0x0059|Nordic Semiconductor ASA|
|---|---|
|0x005A|EM Microelectronic-Marin SA|
|0x005B|Ralink Technology Corporation|
|0x005C|Belkin International, Inc.|
|0x005D|Realtek Semiconductor Corporation|
|0x005E|Stonestreet One, LLC|
|0x005F|Wicentric, Inc.|
|0x0060|RivieraWaves S.A.S|
|0x0061|RDA Microelectronics|
|0x0062|Gibson Guitars|
|0x0063|MiCommand Inc.|
|0x0064|Band XI International, LLC|
|0x0065|HP, Inc.|
|0x0067|GN Hearing|
|0x0068|General Motors|
|0x0069|A&D Engineering, Inc.|
|0x006A|LTIMINDTREE LIMITED|
|0x006B|Polar Electro OY|
|0x006C|Beautiful Enterprise Co., Ltd.|
|0x006E|Summit Data Communications, Inc.|
|0x006F|Sound ID|
|0x0070|Monster, LLC|
|0x0071|connectBlue AB|
|0x0072|ShangHai Super Smart Electronics Co. Ltd.|
|0x0073|Group Sense Ltd.|
|0x0074|Zomm, LLC|
|0x0075|Samsung Electronics Co. Ltd.|
|0x0076|Creative Technology Ltd.|
|0x0077|Laird Connectivity LLC|
|0x0078|Nike, Inc.|
|0x0079|lesswire AG|
|0x007A|MStar Semiconductor, Inc.|
|0x007B|Hanlynn Technologies|
|0x007D|Seers Technology Co., Ltd.|
|0x007F|Autonet Mobile|
|0x0080|DeLorme Publishing Company, Inc.|



Bluetooth SIG Proprietary 

Page 221 of 442 

**Assigned Numbers** / Document 

**2026-06-23** 

|0x0081|WuXi Vimicro|
|---|---|
|0x0082|DSEA A/S|
|0x0083|TimeKeeping Systems, Inc.|
|0x0084|Ludus Helsinki Ltd.|
|0x0085|BlueRadios, Inc.|
|0x0086|Equinux AG|
|0x0087|Garmin International, Inc.|
|0x0089|GN Hearing A/S|
|0x008A|Jawbone|
|0x008B|Topcon Positioning Systems, LLC|
|0x008C|Gimbal Inc.|
|0x008D|Zscan Software|
|0x008E|Quintic Corp|
|0x008F|Telit Wireless Solutions GmbH|
|0x0090|Funai Electric Co., Ltd.|
|0x0091|Advanced PANMOBIL systems GmbH & Co. KG|
|0x0092|ThinkOptics, Inc.|
|0x0093|Universal Electronics, Inc.|
|0x0094|Airoha Technology Corp.|
|0x0095|NEC Lighting, Ltd.|
|0x0096|ODM Technology, Inc.|
|0x0097|ConnecteDevice Ltd.|
|0x0098|zero1.tv GmbH|
|0x0099|i.Tech Dynamic Global Distribution Ltd.|
|0x009A|Alpwise|
|0x009B|Jiangsu Toppower Automotive Electronics Co., Ltd.|
|0x009C|Colorfy, Inc.|
|0x009D|Geoforce Inc.|
|0x009E|Bose Corporation|
|0x009F|Suunto Oy|
|0x00A0|Kensington Computer Products Group|
|0x00A1|SR-Medizinelektronik|
|0x00A2|Vertu Corporation Limited|
|0x00A3|Meta Watch Ltd.|
|0x00A4|LINAK A/S|
|0x00A5|OTL Dynamics LLC|



Bluetooth SIG Proprietary 

Page 222 of 442 

**Assigned Numbers** / Document 

**2026-06-23** 

|0x00A6|Panda Ocean Inc.|
|---|---|
|0x00A7|Visteon Corporation|
|0x00A8|ARP Devices Limited|
|0x00A9|MARELLI EUROPE S.P.A.|
|0x00AA|CAEN RFID srl|
|0x00AB|Ingenieur-Systemgruppe Zahn GmbH|
|0x00AC|Green Throttle Games|
|0x00AD|Peter Systemtechnik GmbH|
|0x00AE|Omegawave Oy|
|0x00AF|Cinetix|
|0x00B0|Passif Semiconductor Corp|
|0x00B1|Saris Cycling Group, Inc|
|0x00B2|Bekey A/S|
|0x00B3|Clarinox Technologies Pty. Ltd.|
|0x00B4|BDE Technology Co., Ltd.|
|0x00B5|Swirl Networks|
|0x00B6|Meso international|
|0x00B7|TreLab Ltd|
|0x00B8|Qualcomm Innovation Center, Inc. (QuIC)|
|0x00B9|Johnson Controls, Inc.|
|0x00BA|Starkey Hearing Technologies|
|0x00BB|S-Power Electronics Limited|
|0x00BC|Ace Sensor Inc|
|0x00BD|Aplix Corporation|
|0x00BE|AAMP of America|
|0x00BF|Stalmart Technology Limited|
|0x00C0|AMICCOM Electronics Corporation|
|0x00C1|Shenzhen Excelsecu Data Technology Co.,Ltd|
|0x00C2|Geneq Inc.|
|0x00C3|adidas AG|
|0x00C4|LG Electronics|
|0x00C5|Onset Computer Corporation|
|0x00C6|Selfly BV|
|0x00C7|Quuppa Oy.|
|0x00C8|GeLo Inc|
|0x00C9|Evluma|



Bluetooth SIG Proprietary 

Page 223 of 442 

**Assigned Numbers** / Document 

**2026-06-23** 

|0x00CA|MC10|
|---|---|
|0x00CB|Binauric SE|
|0x00CC|Beats Electronics|
|0x00CD|Microchip Technology Inc.|
|0x00CE|Eve Systems GmbH|
|0x00CF|ARCHOS SA|
|0x00D0|Dexcom, Inc.|
|0x00D1|Polar Electro Europe B.V.|
|0x00D2|Renesas Design Netherlands B.V.|
|0x00D3|Taixingbang Technology (HK) Co,. LTD.|
|0x00D5|Austco Communication Systems|
|0x00D6|Timex Group USA, Inc.|
|0x00D7|Qualcomm Technologies, Inc.|
|0x00D8|Qualcomm Connected Experiences, Inc.|
|0x00D9|Voyetra Turtle Beach|
|0x00DA|txtr GmbH|
|0x00DB|Snuza (Pty) Ltd|
|0x00DC|Procter & Gamble|
|0x00DD|Hosiden Corporation|
|0x00DE|Muzik LLC|
|0x00DF|Misfit Wearables Corp|
|0x00E0|Google|
|0x00E1|Danlers Ltd|
|0x00E2|Semilink Inc|
|0x00E3|inMusic Brands, Inc|
|0x00E4|L.S. Research, Inc.|
|0x00E5|Eden Software Consultants Ltd.|
|0x00E7|KS Technologies|
|0x00E8|ACTS Technologies|
|0x00E9|Vtrack Systems|
|0x00EA|Nielsen-Kellerman|
|0x00EB|Server Technology Inc.|
|0x00EC|BioResearch Associates|
|0x00ED|Jolly Logic, LLC|
|0x00EE|Above Average Outcomes, Inc.|
|0x00EF|Bitsplitters GmbH|



Bluetooth SIG Proprietary 

Page 224 of 442 

**Assigned Numbers** / Document 

**2026-06-23** 

|0x00F0|PayPal, Inc.|
|---|---|
|0x00F1|Witron Technology Limited|
|0x00F2|Morse Project Inc.|
|0x00F3|Kent Displays Inc.|
|0x00F4|Nautilus Inc.|
|0x00F5|Smartifier Oy|
|0x00F7|VSN Technologies, Inc.|
|0x00F8|AceUni Corp., Ltd.|
|0x00FA|Crystal Alarm AB|
|0x00FB|KOUKAAM a.s.|
|0x00FC|Delphi Corporation|
|0x00FD|ValenceTech Limited|
|0x00FE|Stanley Black and Decker|
|0x00FF|Typo Products, LLC|
|0x0100|TomTom International BV|
|0x0101|Fugoo, Inc.|
|0x0102|Keiser Corporation|
|0x0103|Bang & Olufsen A/S|
|0x0104|PLUS Location Systems Pty Ltd|
|0x0105|Ubiquitous Computing Technology Corporation|
|0x0106|Innovative Yachtter Solutions|
|0x0107|Demant A/S|
|0x0108|Chicony Electronics Co., Ltd.|
|0x0109|Atus BV|
|0x010A|Codegate Ltd|
|0x010B|ERi, Inc|
|0x010C|Transducers Direct, LLC|
|0x010D|DENSO TEN Limited|
|0x010E|Audi AG|
|0x010F|HiSilicon Technologies CO., LIMITED|
|0x0110|Nippon Seiki Co., Ltd.|
|0x0111|Steelseries ApS|
|0x0112|Visybl Inc.|
|0x0113|Openbrain Technologies, Co., Ltd.|
|0x0115|e.solutions|
|0x0116|10AK Technologies|



Bluetooth SIG Proprietary 

Page 225 of 442 

**Assigned Numbers** / Document 

**2026-06-23** 

|0x0117|Wimoto Technologies Inc|
|---|---|
|0x0118|Radius Networks, Inc.|
|0x011A|Qualcomm Labs, Inc.|
|0x011B|Hewlett Packard Enterprise|
|0x011C|Baidu|
|0x011D|Arendi AG|
|0x011E|Skoda Auto a.s.|
|0x011F|Volkswagen AG|
|0x0120|Porsche AG|
|0x0121|Sino Wealth Electronic Ltd.|
|0x0122|AirTurn, Inc.|
|0x0123|Kinsa, Inc|
|0x0124|HID Global|
|0x0125|SEAT es|
|0x0126|Promethean Ltd.|
|0x0127|Salutica Allied Solutions|
|0x0128|GPSI Group Pty Ltd|
|0x0129|Nimble Devices Oy|
|0x012A|Changzhou Yongse Infotech Co., Ltd.|
|0x012B|SportIQ|
|0x012C|TEMEC Instruments B.V.|
|0x012D|Sony Corporation|
|0x012E|ASSA ABLOY|
|0x012F|Clarion Co. Inc.|
|0x0130|Warehouse Innovations|
|0x0131|Cypress Semiconductor|
|0x0132|MADS Inc|
|0x0133|Blue Maestro Limited|
|0x0134|Resolution Products, Ltd.|
|0x0135|Aireware LLC|
|0x0136|Silvair, Inc.|
|0x0137|Prestigio Plaza Ltd.|
|0x0138|NTEO Inc.|
|0x0139|Focus Systems Corporation|
|0x013A|Tencent Holdings Ltd.|
|0x013B|Allegion|



Bluetooth SIG Proprietary 

Page 226 of 442 

**Assigned Numbers** / Document 

**2026-06-23** 

|0x013C|Murata Manufacturing Co., Ltd.|
|---|---|
|0x013D|WirelessWERX|
|0x013E|Nod, Inc.|
|0x0140|Alpine Electronics (China) Co., Ltd|
|0x0141|FedEx Services|
|0x0142|Grape Systems Inc.|
|0x0143|Bkon Connect|
|0x0144|Lintech GmbH|
|0x0145|Novatel Wireless|
|0x0146|Ciright|
|0x0147|Mighty Cast, Inc.|
|0x0148|Ambimat Electronics|
|0x0149|Perytons Ltd.|
|0x014A|Tivoli Audio, LLC|
|0x014B|Master Lock|
|0x014C|Mesh-Net Ltd|
|0x014D|HUIZHOU DESAY SV AUTOMOTIVE CO., LTD.|
|0x014E|Tangerine, Inc.|
|0x014F|B&W Group Ltd.|
|0x0150|Pioneer Corporation|
|0x0151|OnBeep|
|0x0152|Vernier Software & Technology|
|0x0153|ROL Ergo|
|0x0154|Pebble Technology|
|0x0155|NETATMO|
|0x0156|Accumulate AB|
|0x0157|Anhui Huami Information Technology Co., Ltd.|
|0x0158|Inmite s.r.o.|
|0x0159|ChefSteps, Inc.|
|0x015A|micas AG|
|0x015B|Biomedical Research Ltd.|
|0x015C|Pitius Tec S.L.|
|0x015D|Estimote, Inc.|
|0x015E|Unikey Technologies, Inc.|
|0x015F|Timer Cap Co.|
|0x0160|AwoX|



Bluetooth SIG Proprietary 

Page 227 of 442 

**Assigned Numbers** / Document 

**2026-06-23** 

|0x0161|yikes|
|---|---|
|0x0162|MADSGlobalNZ Ltd.|
|0x0163|PCH International|
|0x0164|Qingdao Yeelink Information Technology Co., Ltd.|
|0x0165|Milwaukee Electric Tools|
|0x0166|MISHIK Pte Ltd|
|0x0167|Ascensia Diabetes Care US Inc.|
|0x0168|Spicebox LLC|
|0x0169|emberlight|
|0x016A|Copeland Cold Chain LP|
|0x016B|Qblinks|
|0x016C|MYSPHERA|
|0x016D|LifeScan Inc|
|0x016E|Volantic AB|
|0x016F|Podo Labs, Inc|
|0x0170|Roche Diabetes Care AG|
|0x0171|Amazon.com Services LLC|
|0x0172|Connovate Technology Private Limited|
|0x0173|Kocomojo, LLC|
|0x0174|Everykey Inc.|
|0x0175|Dynamic Controls|
|0x0176|SentriLock|
|0x0177|I-SYST inc.|
|0x0178|CASIO COMPUTER CO., LTD.|
|0x0179|LAPIS Semiconductor Co.,Ltd|
|0x017A|Telemonitor, Inc.|
|0x017B|taskit GmbH|
|0x017C|Mercedes-Benz Group AG|
|0x017D|BatAndCat|
|0x017E|BluDotz Ltd|
|0x017F|XTel Wireless ApS|
|0x0180|Gigaset Technologies GmbH|
|0x0181|Gecko Health Innovations, Inc.|
|0x0183|Walt Disney|
|0x0184|Nectar|
|0x0187|Seraphim Sense Ltd|



Bluetooth SIG Proprietary 

Page 228 of 442 

**Assigned Numbers** / Document 

**2026-06-23** 

|0x0188|Unico RBC|
|---|---|
|0x0189|Physical Enterprises Inc.|
|0x018A|Able Trend Technology Limited|
|0x018B|Konica Minolta, Inc.|
|0x018C|Wilo SE|
|0x018D|Extron Design Services|
|0x018E|Google LLC|
|0x0190|Intelletto Technologies Inc.|
|0x0191|FDK CORPORATION|
|0x0192|Cloudleaf, Inc|
|0x0193|Maveric Automation LLC|
|0x0194|Acoustic Stream Corporation|
|0x0195|Zuli|
|0x0196|Paxton Access Ltd|
|0x0197|WiSilica Inc.|
|0x0198|VENGIT Korlatolt Felelossegu Tarsasag|
|0x0199|SALTO SYSTEMS S.L.|
|0x019A|TRON Forum|
|0x019B|CUBETECH s.r.o.|
|0x019C|Cokiya Incorporated|
|0x019D|CVS Health|
|0x019E|Ceruus|
|0x019F|Strainstall Ltd|
|0x01A0|Channel Enterprises (HK) Ltd.|
|0x01A1|FIAMM|
|0x01A2|GIGALANE.CO.,LTD|
|0x01A4|MSA Innovation, LLC|
|0x01A5|Icon Health and Fitness|
|0x01A6|Wille Engineering|
|0x01A7|ENERGOUS CORPORATION|
|0x01A8|Taobao|
|0x01A9|Canon Inc.|
|0x01AA|Geophysical Technology Inc.|
|0x01AB|Meta Platforms, Inc.|
|0x01AC|Trividia Health, Inc.|
|0x01AD|FlightSafety International|



Bluetooth SIG Proprietary 

Page 229 of 442 

**Assigned Numbers** / Document 

**2026-06-23** 

|0x01AF|Sunrise Micro Devices, Inc.|
|---|---|
|0x01B0|Star Micronics Co., Ltd.|
|0x01B1|Netizens Sp. z o.o.|
|0x01B2|Nymi Inc.|
|0x01B3|Nytec, Inc.|
|0x01B4|Trineo Sp. z o.o.|
|0x01B5|Nest Labs Inc.|
|0x01B6|LM Technologies Ltd|
|0x01B7|General Electric Company|
|0x01B8|i+D3 S.L.|
|0x01B9|HANA Micron|
|0x01BA|SPIA Cycling Inc.|
|0x01BB|Cochlear Bone Anchored Solutions AB|
|0x01BC|SenionLab AB|
|0x01BD|Syszone Co., Ltd|
|0x01BE|Pulsate Mobile Ltd.|
|0x01BF|Hongkong OnMicro Electronics Limited|
|0x01C1|BRADATECH Corp.|
|0x01C2|Transenergooil AG|
|0x01C4|DME Microelectronics|
|0x01C5|Bitcraze AB|
|0x01C6|HASWARE Inc.|
|0x01C7|Abiogenix Inc.|
|0x01C8|Poly-Control ApS|
|0x01C9|Avi-on|
|0x01CA|Laerdal Medical AS|
|0x01CB|Fetch My Pet|
|0x01CC|Sam Labs Ltd.|
|0x01CD|Chengdu Synwing Technology Ltd|
|0x01CE|HOUWA SYSTEM DESIGN, k.k.|
|0x01CF|BSH|
|0x01D0|Primus Inter Pares Ltd|
|0x01D1|August Home, Inc|
|0x01D2|Gill Electronics|
|0x01D3|Sky Wave Design|
|0x01D4|Newlab S.r.l.|



Bluetooth SIG Proprietary 

Page 230 of 442 

**Assigned Numbers** / Document 

**2026-06-23** 

|0x01D5|ELAD srl|
|---|---|
|0x01D6|G-wearables inc.|
|0x01D8|Code Corporation|
|0x01D9|Savant Systems LLC|
|0x01DA|Logitech International SA|
|0x01DB|Innblue Consulting|
|0x01DD|Koninklijke Philips N.V.|
|0x01DE|Minelab Electronics Pty Limited|
|0x01DF|Bison Group Ltd.|
|0x01E0|Widex A/S|
|0x01E1|Jolla Ltd|
|0x01E3|Caterpillar Inc|
|0x01E4|Freedom Innovations|
|0x01E5|Dynamic Devices Ltd|
|0x01E6|Technology Solutions (UK) Ltd|
|0x01EA|Advanced Application Design, Inc.|
|0x01EC|Spreadtrum Communications Shanghai Ltd|
|0x01EE|Valeo Service|
|0x01EF|Fullpower Technologies, Inc.|
|0x01F0|KloudNation|
|0x01F1|Zebra Technologies Corporation|
|0x01F2|Itron, Inc.|
|0x01F3|The University of Tokyo|
|0x01F4|UTC Fire and Security|
|0x01F5|Cool Webthings Limited|
|0x01F6|DJO Global|
|0x01F7|Gelliner Limited|
|0x01F8|Anyka (Guangzhou) Microelectronics Technology Co, LTD|
|0x01F9|Medtronic Inc.|
|0x01FA|Gozio Inc.|
|0x01FB|Form Lifting, LLC|
|0x01FC|Wahoo Fitness, LLC|
|0x01FD|Kontakt Micro-Location Sp. z o.o.|
|0x01FE|Radio Systems Corporation|
|0x01FF|Freescale Semiconductor, Inc.|
|0x0200|Verifone Systems Pte Ltd. Taiwan Branch|



Bluetooth SIG Proprietary 

Page 231 of 442 

**Assigned Numbers** / Document 

**2026-06-23** 

|0x0201|AR Timing|
|---|---|
|0x0202|Rigado LLC|
|0x0203|Kemppi Oy|
|0x0206|Otter Products, LLC|
|0x0207|STEMP Inc.|
|0x0208|LumiGeek LLC|
|0x0209|InvisionHeart Inc.|
|0x020A|Macnica Inc.|
|0x020B|Jaguar Land Rover Limited|
|0x020C|CoroWare Technologies, Inc|
|0x020E|Omron Healthcare Co., LTD|
|0x020F|Comodule GMBH|
|0x0210|ikeGPS|
|0x0211|Telink Semiconductor Co. Ltd|
|0x0212|Interplan Co., Ltd|
|0x0213|Wyler AG|
|0x0214|IK Multimedia Production srl|
|0x0215|Lukoton Experience Oy|
|0x0216|MTI Ltd|
|0x0217|Tech4home, Lda|
|0x0219|DOTT Limited|
|0x021A|Blue Speck Labs, LLC|
|0x021B|Cisco Systems, Inc|
|0x021C|Mobicomm Inc|
|0x021D|Edamic|
|0x021E|Goodnet, Ltd|
|0x021F|Luster Leaf Products Inc|
|0x0220|Manus Machina BV|
|0x0221|Mobiquity Networks Inc|
|0x0222|Praxis Dynamics|
|0x0223|Philip Morris Products S.A.|
|0x0224|Comarch SA|
|0x0225|Nestlé Nespresso S.A.|
|0x0226|Merlinia A/S|
|0x0227|LifeBEAM Technologies|
|0x0228|Twocanoes Labs, LLC|



Bluetooth SIG Proprietary 

Page 232 of 442 

**Assigned Numbers** / Document 

**2026-06-23** 

|0x0229|Muoverti Limited|
|---|---|
|0x022A|Stamer Musikanlagen GMBH|
|0x022B|Tesla, Inc.|
|0x022C|Pharynks Corporation|
|0x022D|Lupine|
|0x022E|Siemens AG|
|0x0230|Foster Electric Company, Ltd|
|0x0231|ETA SA|
|0x0232|x-Senso Solutions Kft|
|0x0233|Shenzhen SuLong Communication Ltd|
|0x0234|FengFan (BeiJing) Technology Co, Ltd|
|0x0235|Qrio Inc|
|0x0236|Pitpatpet Ltd|
|0x0237|MSHeli s.r.l.|
|0x0238|Trakm8 Ltd|
|0x0239|JIN CO, Ltd|
|0x023A|Alatech Tehnology|
|0x023B|Beijing CarePulse Electronic Technology Co, Ltd|
|0x023D|ViCentra B.V.|
|0x023E|Raven Industries|
|0x023F|WaveWare Technologies Inc.|
|0x0240|Argenox Technologies|
|0x0241|Bragi GmbH|
|0x0243|Masimo Corp|
|0x0244|Iotera Inc|
|0x0245|Endress+Hauser|
|0x0246|ACKme Networks, Inc.|
|0x0247|FiftyThree Inc.|
|0x0248|Parker Hannifin Corp|
|0x024A|Uwatec AG|
|0x024B|Orlan LLC|
|0x024C|Blue Clover Devices|
|0x024D|M-Way Solutions GmbH|
|0x024E|Microtronics Engineering GmbH|
|0x024F|Schneider Schreibgeräte GmbH|
|0x0250|Sapphire Circuits LLC|



Bluetooth SIG Proprietary 

Page 233 of 442 

**Assigned Numbers** / Document 

**2026-06-23** 

|0x0251|Lumo Bodytech Inc.|
|---|---|
|0x0252|UKC Technosolution|
|0x0253|Xicato Inc.|
|0x0254|Playbrush|
|0x0255|Dai Nippon Printing Co., Ltd.|
|0x0256|G24 Power Limited|
|0x0257|AdBabble Local Commerce Inc.|
|0x0258|Devialet SA|
|0x0259|ALTYOR|
|0x025A|University of Applied Sciences Valais/Haute Ecole Valaisanne|
|0x025B|Five Interactive, LLC dba Zendo|
|0x025C|NetEase￿Hangzhou￿Network co.Ltd.|
|0x025D|Lexmark International Inc.|
|0x025E|Fluke Corporation|
|0x025F|Yardarm Technologies|
|0x0261|SECVRE GmbH|
|0x0262|Glacial Ridge Technologies|
|0x0263|Identiv, Inc.|
|0x0264|DDS, Inc.|
|0x0265|SMK Corporation|
|0x0266|Schawbel Technologies LLC|
|0x0267|XMI Systems SA|
|0x0268|Cerevo|
|0x0269|Torrox GmbH & Co KG|
|0x026A|Gemalto|
|0x026B|DEKA Research & Development Corp.|
|0x026C|Domster Tadeusz Szydlowski|
|0x026D|Technogym SPA|
|0x026E|FLEURBAEY BVBA|
|0x026F|Aptcode Solutions|
|0x0270|LSI ADL Technology|
|0x0271|Animas Corp|
|0x0272|Alps Alpine Co., Ltd.|
|0x0273|OCEASOFT|
|0x0274|Motsai Research|
|0x0275|Geotab|



Bluetooth SIG Proprietary 

Page 234 of 442 

**Assigned Numbers** / Document 

**2026-06-23** 

|0x0276|E.G.O. Elektro-Geraetebau GmbH|
|---|---|
|0x0277|bewhere inc|
|0x0278|Johnson Outdoors Inc|
|0x0279|steute Schaltgerate GmbH & Co. KG|
|0x027A|Ekomini inc.|
|0x027B|DEFA AS|
|0x027C|Aseptika Ltd|
|0x027D|HUAWEI Technologies Co., Ltd.|
|0x027E|HabitAware, LLC|
|0x027F|ruwido austria gmbh|
|0x0280|ITEC corporation|
|0x0281|StoneL|
|0x0282|Sonova AG|
|0x0283|Maven Machines, Inc.|
|0x0284|Synapse Electronics|
|0x0285|WOWTech Canada Ltd.|
|0x0286|RF Code, Inc.|
|0x0287|Wally Ventures S.L.|
|0x0289|SK Telecom|
|0x028A|Jetro AS|
|0x028B|Code Gears LTD|
|0x028C|NANOLINK APS|
|0x028E|RF Digital Corp|
|0x028F|Church & Dwight Co., Inc|
|0x0290|Multibit Oy|
|0x0291|CliniCloud Inc|
|0x0293|Blue Bite|
|0x0294|ELIAS GmbH|
|0x0295|Sivantos GmbH|
|0x0296|Petzl|
|0x0297|storm power ltd|
|0x0298|EISST Ltd|
|0x0299|Inexess Technology Simma KG|
|0x029A|Currant, Inc.|
|0x029B|C2 Development, Inc.|
|0x029C|Blue Sky Scientific, LLC|



Bluetooth SIG Proprietary 

Page 235 of 442 

**Assigned Numbers** / Document 

**2026-06-23** 

|0x029D|ALOTTAZS LABS, LLC|
|---|---|
|0x029E|Kupson spol. s r.o.|
|0x029F|Areus Engineering GmbH|
|0x02A0|Impossible Camera GmbH|
|0x02A2|Sera4 Ltd.|
|0x02A3|Itude|
|0x02A4|Pacific Lock Company|
|0x02A5|Tendyron Corporation|
|0x02A6|Robert Bosch GmbH|
|0x02A7|Illuxtron international B.V.|
|0x02A8|miSport Ltd.|
|0x02A9|Chargelib|
|0x02AA|Doppler Lab|
|0x02AB|BBPOS Limited|
|0x02AD|Rx Networks, Inc.|
|0x02AE|WeatherFlow, Inc.|
|0x02AF|Technicolor USA Inc.|
|0x02B0|Bestechnic(Shanghai),Ltd|
|0x02B1|Raden Inc|
|0x02B2|Oura Health Oy|
|0x02B3|CLABER S.P.A.|
|0x02B4|Hyginex, Inc.|
|0x02B5|HANSHIN ELECTRIC RAILWAY CO.,LTD.|
|0x02B6|Schneider Electric|
|0x02B7|Oort Technologies LLC|
|0x02B8|Chrono Therapeutics|
|0x02B9|Rinnai Corporation|
|0x02BA|Swissprime Technologies AG|
|0x02BB|YOKOWO CO., LTD.|
|0x02BC|Genevac Ltd|
|0x02BD|Chemtronics|
|0x02BE|Seguro Technology Sp. z o.o.|
|0x02C0|Dash Robotics|
|0x02C1|LINE Corporation|
|0x02C2|Guillemot Corporation|
|0x02C3|Techtronic Power Tools Technology Limited|



Bluetooth SIG Proprietary 

Page 236 of 442 

**Assigned Numbers** / Document 

**2026-06-23** 

|0x02C4|Wilson Sporting Goods|
|---|---|
|0x02C5|Lenovo (Singapore) Pte Ltd.|
|0x02C6|Ayatan Sensors|
|0x02C7|Electronics Tomorrow Limited|
|0x02C8|OneSpan|
|0x02C9|PayRange Inc.|
|0x02CA|ABOV Semiconductor|
|0x02CB|AINA-Wireless Inc.|
|0x02CD|BMA ergonomics b.v.|
|0x02CE|Teva Branded Pharmaceutical Products R&D, Inc.|
|0x02CF|Anima|
|0x02D0|3M|
|0x02D1|Empatica Srl|
|0x02D2|Afero, Inc.|
|0x02D3|Powercast Corporation|
|0x02D4|Secuyou ApS|
|0x02D5|OMRON Corporation|
|0x02D6|Send Solutions|
|0x02D7|NIPPON SYSTEMWARE CO.,LTD.|
|0x02D8|Neosfar|
|0x02D9|Fliegl Agrartechnik GmbH|
|0x02DA|Gilvader|
|0x02DB|Digi International Inc (R)|
|0x02DC|DeWalch Technologies, Inc.|
|0x02DD|Flint Rehabilitation Devices, LLC|
|0x02DE|Samsung SDS Co., Ltd.|
|0x02DF|Blur Product Development|
|0x02E0|University of Michigan|
|0x02E1|Victron Energy BV|
|0x02E2|NTT docomo|
|0x02E3|Carmanah Technologies Corp.|
|0x02E4|Bytestorm Ltd.|
|0x02E5|Espressif Systems (Shanghai) Co., Ltd.|
|0x02E6|Unwire|
|0x02E7|Connected Yard, Inc.|
|0x02E8|American Music Environments|



Bluetooth SIG Proprietary 

Page 237 of 442 

**Assigned Numbers** / Document 

**2026-06-23** 

|0x02E9|Sensogram Technologies, Inc.|
|---|---|
|0x02EA|Fujitsu Limited|
|0x02EB|Ardic Technology|
|0x02EC|Delta Systems, Inc|
|0x02ED|HTC Corporation|
|0x02EE|Citizen Holdings Co., Ltd.|
|0x02EF|SMART-INNOVATION.inc|
|0x02F0|Blackrat Software|
|0x02F1|The Idea Cave, LLC|
|0x02F2|GoPro, Inc.|
|0x02F3|AuthAir, Inc|
|0x02F4|Vensi, Inc.|
|0x02F5|Indagem Tech LLC|
|0x02F6|Intemo Technologies|
|0x02F8|Runteq Oy Ltd|
|0x02F9|IMAGINATION TECHNOLOGIES LTD|
|0x02FB|Clarius Mobile Health Corp.|
|0x02FC|Shanghai Frequen Microelectronics Co., Ltd.|
|0x02FE|Lierda Science & Technology Group Co., Ltd.|
|0x02FF|Silicon Laboratories|
|0x0300|World Moto Inc.|
|0x0301|Giatec Scientific Inc.|
|0x0302|Loop Devices, Inc|
|0x0303|IACA electronique|
|0x0304|Oura Health Ltd|
|0x0305|Swipp ApS|
|0x0306|Life Laboratory Inc.|
|0x0307|FUJI INDUSTRIAL CO.,LTD.|
|0x0308|Surefire, LLC|
|0x0309|Dolby Labs|
|0x030A|Ellisys|
|0x030B|Magnitude Lighting Converters|
|0x030C|Hilti AG|
|0x030D|Devdata S.r.l.|
|0x030F|Shortcut Labs|
|0x0310|SGL Italia S.r.l.|



Bluetooth SIG Proprietary 

Page 238 of 442 

**Assigned Numbers** / Document 

**2026-06-23** 

|0x0311|PEEQ DATA|
|---|---|
|0x0312|Ducere Technologies Pvt Ltd|
|0x0313|DiveNav, Inc.|
|0x0314|RIIG AI Sp. z o.o.|
|0x0315|Thermo Fisher Scientific|
|0x0316|AG Measurematics Pvt. Ltd.|
|0x0317|CHUO Electronics CO., LTD.|
|0x0318|Aspenta International|
|0x0319|Eugster Frismag AG|
|0x031A|Wurth Elektronik eiSos GmbH & Co. KG|
|0x031B|HQ Inc|
|0x031C|Lab Sensor Solutions|
|0x031D|Enterlab ApS|
|0x031E|Eyefi, Inc.|
|0x031F|MetaSystem S.p.A.|
|0x0320|SONO ELECTRONICS. CO., LTD|
|0x0323|Rotor Bike Components|
|0x0324|Astro, Inc.|
|0x0326|Healthwear Technologies (Changzhou)Ltd|
|0x0327|Essex Electronics|
|0x0328|Grundfos A/S|
|0x0329|Eargo, Inc.|
|0x032A|Electronic Design Lab|
|0x032B|ESYLUX|
|0x032C|NIPPON SMT.CO.,Ltd|
|0x032D|BM innovations GmbH|
|0x032E|indoormap|
|0x032F|OttoQ Inc|
|0x0330|North Pole Engineering|
|0x0331|3flares Technologies Inc.|
|0x0333|Mul-T-Lock|
|0x0334|Airthings ASA|
|0x0335|Enlighted Inc|
|0x0336|GISTIC|
|0x0337|AJP2 Holdings, LLC|
|0x0338|COBI GmbH|



Bluetooth SIG Proprietary 

Page 239 of 442 

**Assigned Numbers** / Document 

**2026-06-23** 

|0x0339|Blue Sky Scientific, LLC|
|---|---|
|0x033A|Appception, Inc.|
|0x033B|Courtney Thorne Limited|
|0x033D|TPV Technology Limited|
|0x033E|Monitra SA|
|0x033F|Automation Components, Inc.|
|0x0341|Etesian Technologies LLC|
|0x0342|GERTEC BRASIL LTDA.|
|0x0343|Drekker Development Pty. Ltd.|
|0x0344|Whirl Inc|
|0x0345|Locus Positioning|
|0x0346|Acuity Brands Lighting, Inc|
|0x0347|Prevent Biometrics|
|0x0349|VersaMe|
|0x034B|Libratone A/S|
|0x034C|HM Electronics, Inc.|
|0x034D|TASER International, Inc.|
|0x034F|Heartland Payment Systems|
|0x0350|Bitstrata Systems Inc.|
|0x0351|Pieps GmbH|
|0x0352|iRiding(Xiamen)Technology Co.,Ltd.|
|0x0353|Alpha Audiotronics, Inc.|
|0x0354|TOPPAN FORMS CO.,LTD.|
|0x0355|Sigma Designs, Inc.|
|0x0356|Spectrum Brands, Inc.|
|0x0357|Polymap Wireless|
|0x0358|MagniWare Ltd.|
|0x0359|Novotec Medical GmbH|
|0x035A|Phillips-Medisize A/S|
|0x035B|Matrix Inc.|
|0x035C|Eaton Corporation|
|0x035D|KYS|
|0x035E|Naya Health, Inc.|
|0x035F|Acromag|
|0x0360|Insulet Corporation|
|0x0361|Wellinks Inc.|



Bluetooth SIG Proprietary 

Page 240 of 442 

**Assigned Numbers** / Document 

**2026-06-23** 

|0x0362|ON Semiconductor|
|---|---|
|0x0363|FREELAP SA|
|0x0364|Favero Electronics Srl|
|0x0365|BioMech Sensor LLC|
|0x0366|BOLTT Sports technologies Private limited|
|0x0368|Metormote AB|
|0x0369|littleBits|
|0x036A|SetPoint Medical|
|0x036B|BRControls Products BV|
|0x036C|Zipcar|
|0x036D|AirBolt Pty Ltd|
|0x036E|MOTIVE TECHNOLOGIES, INC.|
|0x036F|Motiv, Inc.|
|0x0370|Wazombi Labs OÜ|
|0x0372|Nixie Labs, Inc.|
|0x0373|AppNearMe Ltd|
|0x0374|Holman Industries|
|0x0375|Expain AS|
|0x0376|Electronic Temperature Instruments Ltd|
|0x0377|Plejd AB|
|0x0378|Propeller Health|
|0x0379|Shenzhen iMCO Electronic Technology Co.,Ltd|
|0x037A|Algoria|
|0x037B|Apption Labs Inc.|
|0x037C|Cronologics Corporation|
|0x037D|MICRODIA Ltd.|
|0x037E|lulabytes S.L.|
|0x037F|Société des Produits Nestlé S.A.|
|0x0380|LLC ”MEGA-F service”|
|0x0381|Sharp Corporation|
|0x0382|Precision Outcomes Ltd|
|0x0383|Kronos Incorporated|
|0x0385|Embedded Electronic Solutions Ltd. dba e2Solutions|
|0x0386|Aterica Inc.|
|0x0387|BluStor PMC, Inc.|
|0x0388|Kapsch TrafficCom AB|



Bluetooth SIG Proprietary 

Page 241 of 442 

**Assigned Numbers** / Document 

**2026-06-23** 

|0x0389|ActiveBlu Corporation|
|---|---|
|0x038A|Kohler Mira Limited|
|0x038B|Noke|
|0x038C|Appion Inc.|
|0x038D|Resmed Ltd|
|0x038E|Crownstone B.V.|
|0x038F|Xiaomi Inc.|
|0x0390|INFOTECH s.r.o.|
|0x0391|Thingsquare AB|
|0x0392|T&D|
|0x0393|LAVAZZA S.p.A.|
|0x0395|SDATAWAY|
|0x0396|BLOKS GmbH|
|0x0397|LEGO System A/S|
|0x0398|Thetatronics Ltd|
|0x0399|Nikon Corporation|
|0x039A|NeST|
|0x039B|South Silicon Valley Microelectronics|
|0x039C|ALE International|
|0x039D|CareView Communications, Inc.|
|0x039E|SchoolBoard Limited|
|0x039F|Molex Corporation|
|0x03A0|IVT Wireless Limited|
|0x03A1|Alpine Labs LLC|
|0x03A2|Candura Instruments|
|0x03A3|SmartMovt Technology Co., Ltd|
|0x03A4|Token Zero Ltd|
|0x03A5|ACE CAD Enterprise Co., Ltd. (ACECAD)|
|0x03A6|Medela, Inc|
|0x03A7|AeroScout|
|0x03A8|Esrille Inc.|
|0x03AA|Exon Sp. z o.o.|
|0x03AB|Meizu Technology Co., Ltd.|
|0x03AD|XiQ|
|0x03AE|Allswell Inc.|
|0x03AF|Comm-N-Sense Corp DBA Verigo|



Bluetooth SIG Proprietary 

Page 242 of 442 

**Assigned Numbers** / Document 

**2026-06-23** 

|0x03B0|VIBRADORM GmbH|
|---|---|
|0x03B1|Otodata Wireless Network Inc.|
|0x03B2|Propagation Systems Limited|
|0x03B3|Midwest Instruments & Controls|
|0x03B4|Alpha Nodus, inc.|
|0x03B5|petPOMM, Inc|
|0x03B6|Mattel|
|0x03B7|Airbly Inc.|
|0x03B8|A-Safe Limited|
|0x03B9|FREDERIQUE CONSTANT SA|
|0x03BA|Maxscend Microelectronics Company Limited|
|0x03BB|Abbott|
|0x03BC|ASB Bank Ltd|
|0x03BD|amadas|
|0x03BE|Applied Science, Inc.|
|0x03BF|iLumi Solutions Inc.|
|0x03C0|Arch Systems Inc.|
|0x03C1|Ember Technologies, Inc.|
|0x03C2|Snapchat Inc|
|0x03C3|Casambi Technologies Oy|
|0x03C4|Pico Technology Inc.|
|0x03C5|St. Jude Medical, Inc.|
|0x03C6|Intricon|
|0x03C7|Structural Health Systems, Inc.|
|0x03C8|Avvel International|
|0x03C9|Gallagher Group|
|0x03CA|In2things Automation Pvt. Ltd.|
|0x03CB|SYSDEV Srl|
|0x03CC|Vonkil Technologies Ltd|
|0x03CD|Wynd Technologies, Inc.|
|0x03CE|CONTRINEX S.A.|
|0x03CF|MIRA, Inc.|
|0x03D0|Watteam Ltd|
|0x03D1|Density Inc.|
|0x03D2|IOT Pot India Private Limited|
|0x03D3|Sigma Connectivity AB|



Bluetooth SIG Proprietary 

Page 243 of 442 

**Assigned Numbers** / Document 

**2026-06-23** 

|0x03D4|PEG PEREGO SPA|
|---|---|
|0x03D5|Wyzelink Systems Inc.|
|0x03D6|Yota Devices LTD|
|0x03D7|FINSECUR|
|0x03D8|Zen-Me Labs Ltd|
|0x03D9|3IWare Co., Ltd.|
|0x03DA|EnOcean GmbH|
|0x03DB|Instabeat, Inc|
|0x03DC|Nima Labs|
|0x03DD|Andreas Stihl AG & Co. KG|
|0x03DE|Nathan Rhoades LLC|
|0x03DF|Grob Technologies, LLC|
|0x03E0|Actions Technology Co.,Ltd|
|0x03E1|SPD Development Company Ltd|
|0x03E2|Sensoan Oy|
|0x03E3|Qualcomm Life Inc|
|0x03E4|Chip-ing AG|
|0x03E5|ffly4u|
|0x03E6|IoT Instruments Oy|
|0x03E7|TRUE Fitness Technology|
|0x03E9|SHENZHEN LEMONJOY TECHNOLOGY CO., LTD.|
|0x03EA|Hello Inc.|
|0x03EB|Ozo Edu, Inc.|
|0x03EC|Jigowatts Inc.|
|0x03ED|BASIC MICRO.COM,INC.|
|0x03EE|CUBE TECHNOLOGIES|
|0x03EF|foolography GmbH|
|0x03F0|CLINK|
|0x03F1|Hestan Smart Cooking Inc.|
|0x03F2|WindowMaster A/S|
|0x03F4|PAL Technologies Ltd|
|0x03F5|WHERE, Inc.|
|0x03F6|Iton Technology Corp.|
|0x03F7|Owl Labs Inc.|
|0x03F8|Rockford Corp.|
|0x03F9|Becon Technologies Co.,Ltd.|



Bluetooth SIG Proprietary 

Page 244 of 442 

**Assigned Numbers** / Document 

**2026-06-23** 

|0x03FA|Vyassoft Technologies Inc|
|---|---|
|0x03FB|Nox Medical|
|0x03FC|Kimberly-Clark|
|0x03FD|Trimble Inc.|
|0x03FE|Littelfuse|
|0x03FF|Withings|
|0x0400|i-developer IT Beratung UG|
|0x0401|Relations Inc.|
|0x0402|Sears Holdings Corporation|
|0x0403|Gantner Electronic GmbH|
|0x0404|Authomate Inc|
|0x0405|Vertex International, Inc.|
|0x0407|Swiss Audio SA|
|0x0408|ToGetHome Inc.|
|0x0409|RYSE INC.|
|0x040A|ZF OPENMATICS s.r.o.|
|0x040B|Jana Care Inc.|
|0x040D|NorthStar Battery Company, LLC|
|0x040E|SKF (U.K.) Limited|
|0x040F|CO-AX Technology, Inc.|
|0x0410|Fender Musical Instruments|
|0x0411|Luidia Inc|
|0x0412|SEFAM|
|0x0413|Wireless Cables Inc|
|0x0416|SODA GmbH|
|0x0417|Fatigue Science|
|0x0419|Novalogy LTD|
|0x041A|Friday Labs Limited|
|0x041B|OrthoAccel Technologies|
|0x041C|WaterGuru, Inc.|
|0x041D|Benning Elektrotechnik und Elektronik GmbH & Co. KG|
|0x041E|Dell Computer Corporation|
|0x041F|Kopin Corporation|
|0x0420|TecBakery GmbH|
|0x0421|Backbone Labs, Inc.|
|0x0422|DELSEY SA|



Bluetooth SIG Proprietary 

Page 245 of 442 

**Assigned Numbers** / Document 

**2026-06-23** 

|0x0423|Chargifi Limited|
|---|---|
|0x0424|Trainesense Ltd.|
|0x0425|Unify Software and Solutions GmbH & Co. KG|
|0x0426|Husqvarna AB|
|0x0427|Focus fleet and fuel management inc|
|0x0428|SmallLoop, LLC|
|0x0429|Prolon Inc.|
|0x042A|BD Medical|
|0x042B|iMicroMed Incorporated|
|0x042C|Ticto N.V.|
|0x042D|Meshtech AS|
|0x042E|MemCachier Inc.|
|0x042F|Danfoss A/S|
|0x0430|SnapStyk Inc.|
|0x0431|Alticor Inc.|
|0x0432|Silk Labs, Inc.|
|0x0433|Pillsy Inc.|
|0x0434|Hatch Baby, Inc.|
|0x0435|Blocks Wearables Ltd.|
|0x0436|Drayson Technologies (Europe) Limited|
|0x0437|eBest IOT Inc.|
|0x0438|Helvar Ltd|
|0x0439|Radiance Technologies|
|0x043A|Nuheara Limited|
|0x043B|Appside co., ltd.|
|0x043D|Coiler Corporation|
|0x043E|Thermomedics, Inc.|
|0x043F|Tentacle Sync GmbH|
|0x0440|Valencell, Inc.|
|0x0442|SECOM CO., LTD.|
|0x0443|Tucker International LLC|
|0x0444|Metanate Limited|
|0x0445|Kobian Canada Inc.|
|0x0446|NETGEAR, Inc.|
|0x0447|Fabtronics Australia Pty Ltd|
|0x0448|Grand Centrix GmbH|



Bluetooth SIG Proprietary 

Page 246 of 442 

**Assigned Numbers** / Document 

**2026-06-23** 

|0x0449|1UP USA.com llc|
|---|---|
|0x044A|SHIMANO INC.|
|0x044B|Nain Inc.|
|0x044C|LifeStyle Lock, LLC|
|0x044D|VEGA Grieshaber KG|
|0x044E|Xtrava Inc.|
|0x044F|TTS Tooltechnic Systems AG & Co. KG|
|0x0450|Teenage Engineering AB|
|0x0451|Tunstall Nordic AB|
|0x0452|Svep Design Center AB|
|0x0453|Qorvo Utrecht B.V.|
|0x0454|Sphinx Electronics GmbH & Co KG|
|0x0456|Nemik Consulting Inc|
|0x0457|RF INNOVATION|
|0x0458|Mini Solution Co., Ltd.|
|0x045A|2048450 Ontario Inc|
|0x045C|Delta T Corporation|
|0x045D|Boston Scientific Corporation|
|0x045E|Nuviz, Inc.|
|0x045F|Real Time Automation, Inc.|
|0x0460|Kolibree|
|0x0461|vhf elektronik GmbH|
|0x0462|Bonsai Systems GmbH|
|0x0463|Fathom Systems Inc.|
|0x0464|Bellman & Symfon Group AB|
|0x0465|International Forte Group LLC|
|0x0467|Codenex Oy|
|0x0468|Kynesim Ltd|
|0x0469|Palago AB|
|0x046A|INSIGMA INC.|
|0x046B|PMD Solutions|
|0x046C|Qingdao Realtime Technology Co., Ltd.|
|0x046D|BEGA Gantenbrink-Leuchten KG|
|0x046E|Pambor Ltd.|
|0x046F|Develco Products A/S|
|0x0470|iDesign s.r.l.|



Bluetooth SIG Proprietary 

Page 247 of 442 

**Assigned Numbers** / Document 

**2026-06-23** 

|0x0471|TiVo Corp|
|---|---|
|0x0472|Control-J Pty Ltd|
|0x0473|Steelcase, Inc.|
|0x0474|iApartment co., ltd.|
|0x0475|Icom inc.|
|0x0477|Blue Spark Technologies|
|0x0478|FarSite Communications Limited|
|0x0479|mywerk system GmbH|
|0x047A|Sinosun Technology Co., Ltd.|
|0x047B|MIYOSHI ELECTRONICS CORPORATION|
|0x047D|Occly LLC|
|0x047E|OurHub Dev IvS|
|0x047F|Pro-Mark, Inc.|
|0x0481|Quintrax Limited|
|0x0482|POS Tuning Udo Vosshenrich GmbH & Co. KG|
|0x0484|Revol Technologies Inc|
|0x0485|SKIDATA AG|
|0x0486|DEV TECNOLOGIA INDUSTRIA, COMERCIO E MANUTENCAO DE<br>EQUIPAMENTOS LTDA. - ME|
|0x0487|Centrica Connected Home|
|0x0488|Automotive Data Solutions Inc|
|0x0489|Igarashi Engineering|
|0x048A|Taelek Oy|
|0x048C|Vectronix AG|
|0x048D|S-Labs Sp. z o.o.|
|0x048E|Companion Medical, Inc.|
|0x048F|BlueKitchen GmbH|
|0x0490|Matting AB|
|0x0491|SOREX - Wireless Solutions GmbH|
|0x0492|ADC Technology, Inc.|
|0x0493|Lynxemi Pte Ltd|
|0x0494|SENNHEISER electronic GmbH & Co. KG|
|0x0496|Polymorphic Labs LLC|
|0x0497|Cochlear Limited|
|0x0498|METER Group, Inc. USA|
|0x0499|Ruuvi Innovations Ltd.|



Bluetooth SIG Proprietary 

Page 248 of 442 

**Assigned Numbers** / Document 

**2026-06-23** 

|0x049A|Situne AS|
|---|---|
|0x049B|nVisti, LLC|
|0x049C|DyOcean|
|0x049D|Uhlmann & Zacher GmbH|
|0x049E|AND!XOR LLC|
|0x049F|Popper Pay AB|
|0x04A2|ovrEngineered, LLC|
|0x04A3|GT-tronics HK Ltd|
|0x04A4|Herbert Waldmann GmbH & Co. KG|
|0x04A5|Guangzhou FiiO Electronics Technology Co.,Ltd|
|0x04A6|Vinetech Co., Ltd|
|0x04A7|Dallas Logic Corporation|
|0x04A8|BioTex, Inc.|
|0x04AA|LINKIO SAS|
|0x04AB|Harbortronics, Inc.|
|0x04AC|Undagrid B.V.|
|0x04AD|Shure Inc|
|0x04AE|ERM Electronic Systems LTD|
|0x04AF|BIOROWER Handelsagentur GmbH|
|0x04B1|Kartographers Technologies Pvt. Ltd.|
|0x04B2|The Shadow on the Moon|
|0x04B3|mobike (Hong Kong) Limited|
|0x04B4|Inuheat Group AB|
|0x04B5|Swiftronix AB|
|0x04B6|Diagnoptics Technologies|
|0x04B7|Analog Devices, Inc.|
|0x04B8|Soraa Inc.|
|0x04B9|CSR Building Products Limited|
|0x04BA|Crestron Electronics, Inc.|
|0x04BB|Neatebox Ltd|
|0x04BC|Draegerwerk AG & Co. KGaA|
|0x04BD|AlbynMedical|
|0x04BE|Averos FZCO|
|0x04BF|VIT Initiative, LLC|
|0x04C0|Statsports International|
|0x04C1|Sospitas, s.r.o.|



Bluetooth SIG Proprietary 

Page 249 of 442 

**Assigned Numbers** / Document 

**2026-06-23** 

|0x04C2|Dmet Products Corp.|
|---|---|
|0x04C3|Mantracourt Electronics Limited|
|0x04C4|TeAM Hutchins AB|
|0x04C5|Seibert Williams Glass, LLC|
|0x04C6|Insta GmbH|
|0x04C7|Svantek Sp. z o.o.|
|0x04C8|Shanghai Flyco Electrical Appliance Co., Ltd.|
|0x04C9|Thornwave Labs Inc|
|0x04CA|Steiner-Optik GmbH|
|0x04CB|Novo Nordisk A/S|
|0x04CD|Safetech Products LLC|
|0x04CE|GOOOLED S.R.L.|
|0x04CF|DOM Sicherheitstechnik GmbH & Co. KG|
|0x04D0|Olympus Corporation|
|0x04D1|KTS GmbH|
|0x04D2|Anloq Technologies Inc.|
|0x04D3|Queercon, Inc|
|0x04D4|TASKA PROSTHETICS LIMITED|
|0x04D5|Gooee Limited|
|0x04D6|LUGLOC LLC|
|0x04D7|Blincam, Inc.|
|0x04D8|FUJIFILM Corporation|
|0x04D9|RM Acquisition LLC|
|0x04DA|Franceschi Marina snc|
|0x04DB|Engineered Audio, LLC.|
|0x04DC|IOTTIVE (OPC) PRIVATE LIMITED|
|0x04DD|4MOD Technology|
|0x04DE|Lutron Electronics Co., Inc.|
|0x04DF|Emerson Electric Co.|
|0x04E0|Guardtec, Inc.|
|0x04E1|REACTEC LIMITED|
|0x04E3|Under Armour|
|0x04E4|Woodenshark|
|0x04E5|Avack Oy|
|0x04E6|Smart Solution Technology, Inc.|
|0x04E8|STABILO International|



Bluetooth SIG Proprietary 

Page 250 of 442 

**Assigned Numbers** / Document 

**2026-06-23** 

|0x04E9|Busch Jaeger Elektro GmbH|
|---|---|
|0x04EA|Pacific Bioscience Laboratories, Inc|
|0x04EB|Bird Home Automation GmbH|
|0x04EC|Motorola Solutions|
|0x04EE|Auxivia|
|0x04EF|DaisyWorks, Inc|
|0x04F0|Kosi Limited|
|0x04F1|Theben AG|
|0x04F2|InDreamer Techsol Private Limited|
|0x04F3|Cerevast Medical|
|0x04F4|ZanCompute Inc.|
|0x04F5|Pirelli Tyre S.P.A.|
|0x04F6|McLear Limited|
|0x04F7|Shenzhen Goodix Technology Co., Ltd|
|0x04F8|Convergence Systems Limited|
|0x04F9|Interactio|
|0x04FA|Androtec GmbH|
|0x04FB|Benchmark Drives GmbH & Co. KG|
|0x04FC|SwingLync L. L. C.|
|0x04FD|Tapkey GmbH|
|0x04FE|Woosim Systems Inc.|
|0x04FF|Microsemi Corporation|
|0x0500|Wiliot LTD.|
|0x0501|Polaris IND|
|0x0502|Specifi-Kali LLC|
|0x0503|Locoroll, Inc|
|0x0504|PHYPLUS Inc|
|0x0505|InPlay, Inc.|
|0x0506|Hager|
|0x0507|Yellowcog|
|0x0508|Axes System sp. z o. o.|
|0x0509|Garage Smart, Inc.|
|0x050A|Shake-on B.V.|
|0x050B|Vibrissa Inc.|
|0x050C|OSRAM GmbH|
|0x050D|TRSystems GmbH|



Bluetooth SIG Proprietary 

Page 251 of 442 

**Assigned Numbers** / Document 

**2026-06-23** 

|0x050E|Yichip Microelectronics (Hangzhou) Co.,Ltd.|
|---|---|
|0x050F|Foundation Engineering LLC|
|0x0510|UNI-ELECTRONICS, INC.|
|0x0511|Brookfield Equinox LLC|
|0x0512|Soprod SA|
|0x0513|9974091 Canada Inc.|
|0x0514|FIBRO GmbH|
|0x0515|RB Controls Co., Ltd.|
|0x0516|Footmarks|
|0x0517|Amtronic Sverige AB|
|0x0518|MAMORIO.inc|
|0x0519|Tyto Life LLC|
|0x051A|Leica Camera AG|
|0x051C|EDPS|
|0x051D|OFF Line Co., Ltd.|
|0x051E|Detect Blue Limited|
|0x051F|Setec Pty Ltd|
|0x0520|Target Corporation|
|0x0521|IAI Corporation|
|0x0522|NS Tech, Inc.|
|0x0523|MTG Co., Ltd.|
|0x0524|Hangzhou iMagic Technology Co., Ltd|
|0x0525|HONGKONG NANO IC TECHNOLOGIES CO., LIMITED|
|0x0526|Honeywell International Inc.|
|0x0527|Albrecht JUNG|
|0x0528|Lunera Lighting Inc.|
|0x0529|Lumen UAB|
|0x052A|Keynes Controls Ltd|
|0x052B|Novartis AG|
|0x052C|Geosatis SA|
|0x052D|EXFO, Inc.|
|0x052E|LEDVANCE GmbH|
|0x052F|Center ID Corp.|
|0x0530|Adolene, Inc.|
|0x0531|D&M Holdings Inc.|
|0x0532|CRESCO Wireless, Inc.|



Bluetooth SIG Proprietary 

Page 252 of 442 

**Assigned Numbers** / Document 

**2026-06-23** 

|0x0533|Nura Operations Pty Ltd|
|---|---|
|0x0534|Frontiergadget, Inc.|
|0x0535|Smart Component Technologies Limited|
|0x0536|ZTR Control Systems LLC|
|0x0537|MetaLogics Corporation|
|0x0538|Medela AG|
|0x0539|OPPLE Lighting Co., Ltd|
|0x053A|Savitech Corp.,|
|0x053B|prodigy|
|0x053C|Screenovate Technologies Ltd|
|0x053D|TESA SA|
|0x053E|CLIM8 LIMITED|
|0x053F|Silergy Corp|
|0x0540|SilverPlus, Inc|
|0x0541|Sharknet srl|
|0x0542|Mist Systems, Inc.|
|0x0543|MIWA LOCK CO.,Ltd|
|0x0544|OrthoSensor, Inc.|
|0x0546|Apexar Technologies S.A.|
|0x0547|LOGICDATA Electronic & Software Entwicklungs GmbH|
|0x0548|Knick Elektronische Messgeraete GmbH & Co. KG|
|0x0549|Smart Technologies and Investment Limited|
|0x054A|Linough Inc.|
|0x054B|Advanced Electronic Designs, Inc.|
|0x054C|Carefree Scott Fetzer Co Inc|
|0x054D|Sensome|
|0x054E|FORTRONIK storitve d.o.o.|
|0x054F|Sinnoz|
|0x0551|Sylero|
|0x0552|Avempace SARL|
|0x0553|Nintendo Co., Ltd.|
|0x0554|National Instruments|
|0x0555|KROHNE Messtechnik GmbH|
|0x0556|Otodynamics Ltd|
|0x0557|Arwin Technology Limited|
|0x0558|benegear, inc.|



Bluetooth SIG Proprietary 

Page 253 of 442 

**Assigned Numbers** / Document 

**2026-06-23** 

|0x0559|Newcon Optik|
|---|---|
|0x055A|CANDY HOUSE, Inc.|
|0x055B|FRANKLIN TECHNOLOGY INC|
|0x055C|Lely|
|0x055D|Valve Corporation|
|0x055E|Hekatron Vertriebs GmbH|
|0x055F|PROTECH S.A.S. DI GIRARDI ANDREA & C.|
|0x0560|Sarita CareTech APS|
|0x0561|Finder S.p.A.|
|0x0562|Thalmic Labs Inc.|
|0x0563|Steinel GmbH|
|0x0564|Beghelli Spa|
|0x0566|CORE TRANSPORT TECHNOLOGIES NZ LIMITED|
|0x0567|Xiamen Everesports Goods Co., Ltd|
|0x0568|Bodyport Inc.|
|0x056A|Flipnavi Co.,Ltd.|
|0x056B|Rion Co., Ltd.|
|0x056C|Long Range Systems, LLC|
|0x056D|Redmond Industrial Group LLC|
|0x056E|VIZPIN INC.|
|0x056F|BikeFinder AS|
|0x0570|Consumer Sleep Solutions LLC|
|0x0571|PSIKICK, INC.|
|0x0572|AntTail.com|
|0x0573|Lighting Science Group Corp.|
|0x0574|AFFORDABLE ELECTRONICS INC|
|0x0575|Integral Memroy Plc|
|0x0576|Globalstar, Inc.|
|0x0577|True Wearables, Inc.|
|0x0578|Wellington Drive Technologies Ltd|
|0x057A|OMNI Remotes|
|0x057B|Duracell U.S. Operations Inc.|
|0x057C|Toor Technologies LLC|
|0x057D|Instinct Performance|
|0x057E|Beco, Inc|
|0x057F|Scuf Gaming International, LLC|



Bluetooth SIG Proprietary 

Page 254 of 442 

**Assigned Numbers** / Document 

**2026-06-23** 

|0x0581|LYS TECHNOLOGIES LTD|
|---|---|
|0x0582|Breakwall Analytics, LLC|
|0x0583|Code Blue Communications|
|0x0584|Gira Giersiepen GmbH & Co. KG|
|0x0585|Hearing Lab Technology|
|0x0586|LEGRAND|
|0x0587|Derichs GmbH|
|0x0588|ALT-TEKNIK LLC|
|0x0589|Star Technologies|
|0x058A|START TODAY CO.,LTD.|
|0x058B|Maxim Integrated Products|
|0x058C|Fracarro Radioindustrie SRL|
|0x058D|Jungheinrich Aktiengesellschaft|
|0x058E|Meta Platforms Technologies, LLC|
|0x058F|HENDON SEMICONDUCTORS PTY LTD|
|0x0590|Pur3 Ltd|
|0x0591|Viasat Group S.p.A.|
|0x0592|IZITHERM|
|0x0593|Spaulding Clinical Research|
|0x0594|Kohler Company|
|0x0595|Inor Process AB|
|0x0596|My Smart Blinds|
|0x0597|RadioPulse Inc|
|0x0598|rapitag GmbH|
|0x0599|Lazlo326, LLC.|
|0x059A|Teledyne Lecroy, Inc.|
|0x059B|Dataflow Systems Limited|
|0x059C|Macrogiga Electronics|
|0x059D|Tandem Diabetes Care|
|0x059E|Polycom, Inc.|
|0x059F|Fisher & Paykel Healthcare|
|0x05A0|Dream Devices Technologies Oy|
|0x05A1|Shanghai Xiaoyi Technology Co.,Ltd.|
|0x05A2|ADHERIUM(NZ) LIMITED|
|0x05A3|Axiomware Systems Incorporated|
|0x05A4|O. E. M. Controls, Inc.|



Bluetooth SIG Proprietary 

Page 255 of 442 

**Assigned Numbers** / Document 

**2026-06-23** 

|0x05A5|Kiiroo BV|
|---|---|
|0x05A6|Telecon Mobile Limited|
|0x05A7|Sonos Inc|
|0x05A8|Tom Allebrandi Consulting|
|0x05A9|Monidor|
|0x05AA|Tramex Limited|
|0x05AB|Nofence AS|
|0x05AC|GoerTek Dynaudio Co., Ltd.|
|0x05AD|INIA|
|0x05AE|CARMATE MFG.CO.,LTD|
|0x05AF|OV LOOP, INC.|
|0x05B0|NewTec GmbH|
|0x05B1|Medallion Instrumentation Systems|
|0x05B2|CAREL INDUSTRIES S.P.A.|
|0x05B3|Parabit Systems, Inc.|
|0x05B4|White Horse Scientific ltd|
|0x05B5|verisilicon|
|0x05B6|Elecs Industry Co.,Ltd.|
|0x05B7|Beijing Pinecone Electronics Co.,Ltd.|
|0x05B8|Ambystoma Labs Inc.|
|0x05B9|Suzhou Pairlink Network Technology|
|0x05BA|igloohome|
|0x05BB|Oxford Metrics plc|
|0x05BC|Leviton Mfg. Co., Inc.|
|0x05BD|ULC Robotics Inc.|
|0x05BF|Real-World-Systems Corporation|
|0x05C0|Nalu Medical, Inc.|
|0x05C1|P.I.Engineering|
|0x05C2|Grote Industries|
|0x05C3|Runtime, Inc.|
|0x05C4|Codecoup sp. z o.o. sp. k.|
|0x05C5|SELVE GmbH & Co. KG|
|0x05C7|Lippert Components, INC|
|0x05C8|SOMFY SAS|
|0x05C9|TBS Electronics B.V.|
|0x05CA|MHL Custom Inc|



Bluetooth SIG Proprietary 

Page 256 of 442 

**Assigned Numbers** / Document 

**2026-06-23** 

|0x05CB|LucentWear LLC|
|---|---|
|0x05CC|WATTS ELECTRONICS|
|0x05CD|RJ Brands LLC|
|0x05CE|V-ZUG Ltd|
|0x05CF|Biowatch SA|
|0x05D0|Anova Applied Electronics|
|0x05D1|Lindab AB|
|0x05D2|frogblue TECHNOLOGY GmbH|
|0x05D3|Acurable Limited|
|0x05D4|LAMPLIGHT Co., Ltd.|
|0x05D5|TEGAM, Inc.|
|0x05D6|Zhuhai Jieli technology Co.,Ltd|
|0x05D7|modum.io AG|
|0x05D8|Farm Jenny LLC|
|0x05D9|Toyo Electronics Corporation|
|0x05DA|Applied Neural Research Corp|
|0x05DB|Avid Identification Systems, Inc.|
|0x05DC|Petronics Inc.|
|0x05DD|essentim GmbH|
|0x05DE|QT Medical INC.|
|0x05DF|VIRTUALCLINIC.DIRECT LIMITED|
|0x05E0|Viper Design LLC|
|0x05E1|Human, Incorporated|
|0x05E2|stAPPtronics GmbH|
|0x05E3|Elemental Machines, Inc.|
|0x05E4|Taiyo Yuden Co., Ltd|
|0x05E5|INEO ENERGY& SYSTEMS|
|0x05E6|Motion Instruments Inc.|
|0x05E7|PressurePro|
|0x05E8|COWBOY|
|0x05E9|iconmobile GmbH|
|0x05EA|ACS-Control-System GmbH|
|0x05EB|Bayerische Motoren Werke AG|
|0x05EC|Gycom Svenska AB|
|0x05ED|Fuji Xerox Co., Ltd|
|0x05EF|SIKOM AS|



Bluetooth SIG Proprietary 

Page 257 of 442 

**Assigned Numbers** / Document 

**2026-06-23** 

|0x05F0|beken|
|---|---|
|0x05F1|The Linux Foundation|
|0x05F2|Try and E CO.,LTD.|
|0x05F3|SeeScan|
|0x05F4|Clearity, LLC|
|0x05F5|GS TAG|
|0x05F6|DPTechnics|
|0x05F7|TRACMO, INC.|
|0x05F8|Anki Inc.|
|0x05F9|Hagleitner Hygiene International GmbH|
|0x05FA|Konami Sports Life Co., Ltd.|
|0x05FB|Arblet Inc.|
|0x05FC|Masbando GmbH|
|0x05FD|Innoseis|
|0x05FE|Niko nv|
|0x05FF|Wellnomics Ltd|
|0x0600|iRobot Corporation|
|0x0601|Schrader Electronics|
|0x0602|Geberit International AG|
|0x0603|Fourth Evolution Inc|
|0x0605|FMW electronic Futterer u. Maier-Wolf OHG|
|0x0606|John Deere|
|0x0607|Rookery Technology Ltd|
|0x0608|KeySafe-Cloud|
|0x0609|BUCHI Labortechnik AG|
|0x060A|IQAir AG|
|0x060B|Triax Technologies Inc|
|0x060C|Vuzix Corporation|
|0x060D|TDK Corporation|
|0x060E|Blueair AB|
|0x060F|Signify Netherlands B.V.|
|0x0610|ADH GUARDIAN USA LLC|
|0x0611|Beurer GmbH|
|0x0612|Playfinity AS|
|0x0613|Hans Dinslage GmbH|
|0x0614|OnAsset Intelligence, Inc.|



Bluetooth SIG Proprietary 

Page 258 of 442 

**Assigned Numbers** / Document 

**2026-06-23** 

|0x0615|INTER ACTION Corporation|
|---|---|
|0x0616|OS42 UG (haftungsbeschraenkt)|
|0x0618|Audio-Technica Corporation|
|0x0619|Six Guys Labs, s.r.o.|
|0x061A|R.W. Beckett Corporation|
|0x061B|silex technology, inc.|
|0x061C|Univations Limited|
|0x061D|SENS Innovation ApS|
|0x061E|Diamond Kinetics, Inc.|
|0x061F|Phrame Inc.|
|0x0620|Forciot Oy|
|0x0621|Noordung d.o.o.|
|0x0622|Beam Labs, LLC|
|0x0624|Biovotion AG|
|0x0625|Square Panda, Inc.|
|0x0626|Amplifico|
|0x0627|WEG S.A.|
|0x0628|Ensto Oy|
|0x0629|PHONEPE PVT LTD|
|0x062B|MinebeaMitsumi Inc.|
|0x062C|ASPion GmbH|
|0x062D|Vossloh-Schwabe Deutschland GmbH|
|0x062E|Procept|
|0x062F|ONKYO Corporation|
|0x0630|Asthrea D.O.O.|
|0x0631|Fortiori Design LLC|
|0x0632|Hugo Muller GmbH & Co KG|
|0x0633|Wangi Lai PLT|
|0x0634|Fanstel Corp|
|0x0635|Crookwood|
|0x0636|ELECTRONICA INTEGRAL DE SONIDO S.A.|
|0x0637|GiP Innovation Tools GmbH|
|0x0638|LX SOLUTIONS PTY LIMITED|
|0x0639|Shenzhen Minew Technologies Co., Ltd.|
|0x063A|Prolojik Limited|
|0x063B|Kromek Group Plc|



Bluetooth SIG Proprietary 

Page 259 of 442 

**Assigned Numbers** / Document 

**2026-06-23** 

|0x063C|Contec Medical Systems Co., Ltd.|
|---|---|
|0x063D|Xradio Technology Co.,Ltd.|
|0x063E|The Indoor Lab, LLC|
|0x063F|LDL TECHNOLOGY|
|0x0640|Dish Network LLC|
|0x0641|Revenue Collection Systems FRANCE SAS|
|0x0642|Bluetrum Technology Co.,Ltd|
|0x0643|makita corporation|
|0x0644|Apogee Instruments|
|0x0645|BM3|
|0x0646|SGV Group Holding GmbH & Co. KG|
|0x0647|MED-EL|
|0x0648|Ultune Technologies|
|0x0649|Ryeex Technology Co.,Ltd.|
|0x064A|Open Research Institute, Inc.|
|0x064B|Scale-Tec, Ltd|
|0x064C|Zumtobel Group AG|
|0x064D|iLOQ Oy|
|0x064E|KRUXWorks Technologies Private Limited|
|0x064F|Digital Matter Pty Ltd|
|0x0650|Coravin, Inc.|
|0x0651|Stasis Labs, Inc.|
|0x0652|ITZ Innovations- und Technologiezentrum GmbH|
|0x0653|Meggitt SA|
|0x0654|Ledlenser GmbH & Co. KG|
|0x0655|Renishaw PLC|
|0x0656|ZhuHai AdvanPro Technology Company Limited|
|0x0657|Meshtronix Limited|
|0x0658|Payex Norge AS|
|0x0659|UnSeen Technologies Oy|
|0x065A|Marshall Group AB|
|0x065B|Sesam Solutions BV|
|0x065C|PixArt Imaging Inc.|
|0x065D|Panduit Corp.|
|0x065E|Alo AB|
|0x065F|Ricoh Company Ltd|



Bluetooth SIG Proprietary 

Page 260 of 442 

**Assigned Numbers** / Document 

**2026-06-23** 

|0x0660|RTC Industries, Inc.|
|---|---|
|0x0661|Mode Lighting Limited|
|0x0662|Particle Industries, Inc.|
|0x0663|Advanced Telemetry Systems, Inc.|
|0x0664|RHA TECHNOLOGIES LTD|
|0x0665|Pure International Limited|
|0x0666|WTO Werkzeug-Einrichtungen GmbH|
|0x0667|Spark Technology Labs Inc.|
|0x0668|Bleb Technology srl|
|0x0669|Livanova USA, Inc.|
|0x066A|Brady Worldwide Inc.|
|0x066B|DewertOkin GmbH|
|0x066C|Ztove ApS|
|0x066D|Venso EcoSolutions AB|
|0x066E|Eurotronik Kranj d.o.o.|
|0x066F|Hug Technology Ltd|
|0x0670|Gema Switzerland GmbH|
|0x0671|Buzz Products Ltd.|
|0x0672|Kopi|
|0x0673|Innova Ideas Limited|
|0x0674|BeSpoon|
|0x0676|Expai Solutions Private Limited|
|0x0677|Innovation First, Inc.|
|0x0678|SABIK Offshore GmbH|
|0x0679|4iiii Innovations Inc.|
|0x067A|The Energy Conservatory, Inc.|
|0x067B|I.FARM, INC.|
|0x067C|Tile, Inc.|
|0x067D|Form Athletica Inc.|
|0x067F|NETGRID S.N.C. DI BISSOLI MATTEO, CAMPOREALE SIMONE,<br>TOGNETTI FEDERICO|
|0x0680|Mannkind Corporation|
|0x0681|Trade FIDES a.s.|
|0x0682|Photron Limited|
|0x0683|Eltako GmbH|
|0x0684|Dermalapps, LLC|



Bluetooth SIG Proprietary 

Page 261 of 442 

**Assigned Numbers** / Document 

**2026-06-23** 

|0x0685|Greenwald Industries|
|---|---|
|0x0686|inQs Co., Ltd.|
|0x0687|Cherry GmbH|
|0x0688|Amsted Digital Solutions Inc.|
|0x0689|Tacx b.v.|
|0x068A|Raytac Corporation|
|0x068B|Jiangsu Teranovo Tech Co., Ltd.|
|0x068E|Razer Inc.|
|0x068F|JRM Group Limited|
|0x0690|Eccrine Systems, Inc.|
|0x0691|Curie Point AB|
|0x0692|Georg Fischer AG|
|0x0693|Hach - Danaher|
|0x0694|T&A Laboratories LLC|
|0x0695|Koki Holdings Co., Ltd.|
|0x0696|Gunakar Private Limited|
|0x0697|Stemco Products Inc|
|0x0698|Wood IT Security, LLC|
|0x0699|RandomLab SAS|
|0x069A|Adero, Inc.|
|0x069B|Dragonchip Limited|
|0x069C|Noomi AB|
|0x069E|Delta Electronics, Inc.|
|0x069F|FlowMotion Technologies AS|
|0x06A0|OBIQ Location Technology Inc.|
|0x06A1|Cardo Systems, Ltd|
|0x06A2|Globalworx GmbH|
|0x06A3|Nymbus, LLC|
|0x06A4|LIMNO Co. Ltd.|
|0x06A5|TEKZITEL PTY LTD|
|0x06A6|Roambee Corporation|
|0x06A7|Chipsea Technologies (ShenZhen) Corp.|
|0x06A8|GD Midea Air-Conditioning Equipment Co., Ltd.|
|0x06A9|Soundmax Electronics Limited|
|0x06AA|Produal Oy|
|0x06AB|HMS Industrial Networks AB|



Bluetooth SIG Proprietary 

Page 262 of 442 

**Assigned Numbers** / Document 

**2026-06-23** 

|0x06AC|Ingchips Technology Co., Ltd.|
|---|---|
|0x06AD|InnovaSea Systems Inc.|
|0x06AE|SenseQ Inc.|
|0x06AF|Shoof Technologies|
|0x06B0|BRK Brands, Inc.|
|0x06B1|SimpliSafe, Inc.|
|0x06B2|Tussock Innovation 2013 Limited|
|0x06B4|Sencilion Oy|
|0x06B5|Wabilogic Ltd.|
|0x06B6|Sociometric Solutions, Inc.|
|0x06B7|iCOGNIZE GmbH|
|0x06B8|ShadeCraft, Inc|
|0x06B9|Beflex Inc.|
|0x06BA|Beaconzone Ltd|
|0x06BB|Leaftronix Analogic Solutions Private Limited|
|0x06BC|TWS Srl|
|0x06BD|ABB Oy|
|0x06BE|HitSeed Oy|
|0x06C0|CAME S.p.A.|
|0x06C1|Alarm.com Holdings, Inc|
|0x06C2|Measurlogic Inc.|
|0x06C3|King I Electronics.Co.,Ltd|
|0x06C4|Dream Labs GmbH|
|0x06C5|Urban Compass, Inc|
|0x06C6|Simm Tronic Limited|
|0x06C8|Storz & Bickel GmbH & Co. KG|
|0x06C9|MYLAPS B.V.|
|0x06CA|Shenzhen Zhongguang Infotech Technology Development Co., Ltd|
|0x06CB|Dyeware, LLC|
|0x06CC|Dongguan SmartAction Technology Co.,Ltd.|
|0x06CD|DIG Corporation|
|0x06CE|FIOR & GENTZ|
|0x06D0|Etekcity Corporation|
|0x06D1|Meyer Sound Laboratories, Incorporated|
|0x06D2|CeoTronics AG|
|0x06D5|Sensirion AG|



Bluetooth SIG Proprietary 

Page 263 of 442 

**Assigned Numbers** / Document 

**2026-06-23** 

|0x06D6|JCT Healthcare Pty Ltd|
|---|---|
|0x06D7|FUBA Automotive Electronics GmbH|
|0x06D8|AW Company|
|0x06D9|Shanghai Mountain View Silicon Co.,Ltd.|
|0x06DA|Zliide Technologies ApS|
|0x06DB|Automatic Labs, Inc.|
|0x06DC|Industrial Network Controls, LLC|
|0x06DD|Intellithings Ltd.|
|0x06DE|Navcast, Inc.|
|0x06DF|HLI Solutions Inc.|
|0x06E0|Avaya Inc.|
|0x06E1|Milestone AV Technologies LLC|
|0x06E2|Alango Technologies Ltd|
|0x06E3|Spinlock Ltd|
|0x06E4|Aluna|
|0x06E5|OPTEX CO.,LTD.|
|0x06E6|NIHON DENGYO KOUSAKU|
|0x06E7|VELUX A/S|
|0x06E8|Almendo Technologies GmbH|
|0x06E9|Zmartfun Electronics, Inc.|
|0x06EA|SafeLine Sweden AB|
|0x06EB|Houston Radar LLC|
|0x06ED|J Neades Ltd|
|0x06EF|ALCARE Co., Ltd.|
|0x06F0|Chargy Technologies, SL|
|0x06F1|Shibutani Co., Ltd.|
|0x06F2|Trapper Data AB|
|0x06F3|Alfred International Inc.|
|0x06F4|Touché Technology Ltd|
|0x06F5|Vigil Technologies Inc.|
|0x06F6|Vitulo Plus BV|
|0x06F7|WILKA Schliesstechnik GmbH|
|0x06F8|BodyPlus Technology Co.,Ltd|
|0x06F9|happybrush GmbH|
|0x06FA|Enequi AB|
|0x06FB|Sartorius AG|



Bluetooth SIG Proprietary 

Page 264 of 442 

**Assigned Numbers** / Document 

**2026-06-23** 

|0x06FC|Tom Communication Industrial Co.,Ltd.|
|---|---|
|0x06FD|ESS Embedded System Solutions Inc.|
|0x06FE|Mahr GmbH|
|0x06FF|Redpine Signals Inc|
|0x0700|TraqFreq LLC|
|0x0701|PAFERS TECH|
|0x0702|Akciju sabiedriba ”SAF TEHNIKA”|
|0x0703|Beijing Jingdong Century Trading Co., Ltd.|
|0x0704|JBX Designs Inc.|
|0x0705|AB Electrolux|
|0x0706|Wernher von Braun Center for ASdvanced Research|
|0x0707|Essity Hygiene and Health Aktiebolag|
|0x0708|Be Interactive Co., Ltd|
|0x0709|Carewear Corp.|
|0x070A|Huf Hülsbeck & Fürst GmbH & Co. KG|
|0x070B|Element Products, Inc.|
|0x070C|Beijing Winner Microelectronics Co.,Ltd|
|0x070D|SmartSnugg Pty Ltd|
|0x070E|FiveCo Sarl|
|0x070F|California Things Inc.|
|0x0710|Audiodo AB|
|0x0711|ABAX AS|
|0x0712|Bull Group Company Limited|
|0x0713|Respiri Limited|
|0x0714|MindPeace Safety LLC|
|0x0715|MBARC LABS Inc|
|0x0716|Altonics|
|0x0718|IDIBAIX enginneering|
|0x0719|COREIOT PTY LTD|
|0x071A|REVSMART WEARABLE HK CO LTD|
|0x071B|Precor|
|0x071C|F5 Sports, Inc|
|0x071D|exoTIC Systems|
|0x071E|DONGGUAN HELE ELECTRONICS CO., LTD|
|0x071F|Dongguan Liesheng Electronic Co.Ltd|
|0x0720|Oculeve, Inc.|



Bluetooth SIG Proprietary 

Page 265 of 442 

**Assigned Numbers** / Document 

**2026-06-23** 

|0x0721|Clover Network, Inc.|
|---|---|
|0x0722|Xiamen Eholder Electronics Co.Ltd|
|0x0723|Ford Motor Company|
|0x0724|Guangzhou SuperSound Information Technology Co.,Ltd|
|0x0725|Tedee Sp. z o.o.|
|0x0726|PHC Corporation|
|0x0728|Eli Lilly and Company|
|0x0729|SwaraLink Technologies|
|0x072A|JMR embedded systems GmbH|
|0x072B|Bitkey Inc.|
|0x072C|GWA Hygiene GmbH|
|0x072D|Safera Oy|
|0x072E|Open Platform Systems LLC|
|0x072F|OnePlus Electronics (Shenzhen) Co., Ltd.|
|0x0730|Wildlife Acoustics, Inc.|
|0x0731|ABLIC Inc.|
|0x0732|Dairy Tech, LLC|
|0x0733|Iguanavation, Inc.|
|0x0734|DiUS Computing Pty Ltd|
|0x0735|UpRight Technologies LTD|
|0x0736|Luna XIO, Inc.|
|0x0737|LLC Navitek|
|0x0738|Glass Security Pte Ltd|
|0x0739|Jiangsu Qinheng Co., Ltd.|
|0x073A|Chandler Systems Inc.|
|0x073B|Fantini Cosmi s.p.a.|
|0x073D|Beijing Hao Heng Tian Tech Co., Ltd.|
|0x073E|Bluepack S.R.L.|
|0x073F|Beijing Unisoc Technologies Co., Ltd.|
|0x0741|MAC SRL|
|0x0742|DML LLC|
|0x0743|Sanofi|
|0x0744|SOCOMEC|
|0x0745|WIZNOVA, Inc.|
|0x0746|Seitec Elektronik GmbH|
|0x0747|OR Technologies Pty Ltd|



Bluetooth SIG Proprietary 

Page 266 of 442 

**Assigned Numbers** / Document 

**2026-06-23** 

|0x0748|GuangZhou KuGou Computer Technology Co.Ltd|
|---|---|
|0x0749|DIAODIAO (Beijing) Technology Co., Ltd.|
|0x074A|Illusory Studios LLC|
|0x074B|Sarvavid Software Solutions LLP|
|0x074D|Amtech Systems, LLC|
|0x074E|EAGLE DETECTION SA|
|0x074F|MEDIATECH S.R.L.|
|0x0750|Hamilton Professional Services of Canada Incorporated|
|0x0751|Changsha JEMO IC Design Co.,Ltd|
|0x0752|Elatec GmbH|
|0x0753|JLG Industries, Inc.|
|0x0754|Michael Parkin|
|0x0755|Brother Industries, Ltd|
|0x0756|Lumens For Less, Inc|
|0x0757|ELA Innovation|
|0x0758|umanSense AB|
|0x0759|Shanghai InGeek Cyber Security Co., Ltd.|
|0x075A|HARMAN CO.,LTD.|
|0x075B|Smart Sensor Devices AB|
|0x075C|Antitronics Inc.|
|0x075D|RHOMBUS SYSTEMS, INC.|
|0x075E|Katerra Inc.|
|0x075F|Remote Solution Co., LTD.|
|0x0760|Vimar SpA|
|0x0761|Mantis Tech LLC|
|0x0762|TerOpta Ltd|
|0x0763|PIKOLIN S.L.|
|0x0764|WWZN Information Technology Company Limited|
|0x0765|Voxx International|
|0x0766|ART AND PROGRAM, INC.|
|0x0767|NITTO DENKO ASIA TECHNICAL CENTRE PTE. LTD.|
|0x0768|Peloton Interactive Inc.|
|0x0769|Force Impact Technologies|
|0x076A|Dmac Mobile Developments, LLC|
|0x076B|Engineered Medical Technologies|
|0x076C|Noodle Technology inc|



Bluetooth SIG Proprietary 

Page 267 of 442 

**Assigned Numbers** / Document 

**2026-06-23** 

|0x076D|Graesslin GmbH|
|---|---|
|0x076E|WuQi technologies, Inc.|
|0x076F|Successful Endeavours Pty Ltd|
|0x0770|InnoCon Medical ApS|
|0x0771|Corvex Connected Safety|
|0x0772|Thirdwayv Inc.|
|0x0774|C-MAX Asia Limited|
|0x0775|4eBusiness GmbH|
|0x0776|Cyber Transport Control GmbH|
|0x0777|Cue|
|0x0778|KOAMTAC INC.|
|0x0779|Loopshore Oy|
|0x077A|Niruha Systems Private Limited|
|0x077C|radius co., ltd.|
|0x077D|Sensority, s.r.o.|
|0x077E|Sparkage Inc.|
|0x077F|Glenview Software Corporation|
|0x0780|Finch Technologies Ltd.|
|0x0781|Qingping Technology (Beijing) Co., Ltd.|
|0x0782|DeviceDrive AS|
|0x0783|ESEMBER LIMITED LIABILITY COMPANY|
|0x0784|audifon GmbH & Co. KG|
|0x0785|O2 Micro, Inc.|
|0x0786|HLP Controls Pty Limited|
|0x0788|BubblyNet, LLC|
|0x0789|PCB Piezotronics, Inc.|
|0x078A|The Wildflower Foundation|
|0x078B|Optikam Tech Inc.|
|0x078C|MINIBREW HOLDING B.V|
|0x078D|Cybex GmbH|
|0x078E|FUJIMIC NIIGATA, INC.|
|0x078F|Hanna Instruments, Inc.|
|0x0790|KOMPAN A/S|
|0x0791|Scosche Industries, Inc.|
|0x0792|Cricut, Inc.|
|0x0793|AEV spol. s r.o.|



Bluetooth SIG Proprietary 

Page 268 of 442 

**Assigned Numbers** / Document 

**2026-06-23** 

|0x0794|The Coca-Cola Company|
|---|---|
|0x0795|GASTEC CORPORATION|
|0x0796|StarLeaf Ltd|
|0x0797|Water-i.d. GmbH|
|0x0798|HoloKit, Inc.|
|0x0799|PlantChoir Inc.|
|0x079A|GuangDong Oppo Mobile Telecommunications Corp., Ltd.|
|0x079B|CST ELECTRONICS (PROPRIETARY) LIMITED|
|0x079C|Sky UK Limited|
|0x079D|Digibale Pty Ltd|
|0x079E|Smartloxx GmbH|
|0x079F|Pune Scientific LLP|
|0x07A0|Regent Beleuchtungskorper AG|
|0x07A2|Roku, Inc.|
|0x07A4|Xiamen Mage Information Technology Co., Ltd.|
|0x07A5|RAB Lighting, Inc.|
|0x07A6|Musen Connect, Inc.|
|0x07A7|Zume, Inc.|
|0x07A8|conbee GmbH|
|0x07A9|Bruel & Kjaer Sound & Vibration|
|0x07AA|The Kroger Co.|
|0x07AB|Granite River Solutions, Inc.|
|0x07AC|LoupeDeck Oy|
|0x07AD|New H3C Technologies Co.,Ltd|
|0x07AE|Aurea Solucoes Tecnologicas Ltda.|
|0x07AF|Hong Kong Bouffalo Lab Limited|
|0x07B0|GV Concepts Inc.|
|0x07B1|Thomas Dynamics, LLC|
|0x07B2|Moeco IOT Inc.|
|0x07B3|2N TELEKOMUNIKACE a.s.|
|0x07B4|Hormann KG Antriebstechnik|
|0x07B5|CRONO CHIP, S.L.|
|0x07B6|Soundbrenner Limited|
|0x07B7|ETABLISSEMENTS GEORGES RENAULT|
|0x07B8|iSwip|
|0x07BA|Battery-Biz Inc.|



Bluetooth SIG Proprietary 

Page 269 of 442 

**Assigned Numbers** / Document 

**2026-06-23** 

|0x07BB|EPIC S.R.L.|
|---|---|
|0x07BD|Genedrive Diagnostics Ltd|
|0x07BE|Axentia Technologies AB|
|0x07BF|REGULA Ltd.|
|0x07C0|Biral AG|
|0x07C2|Radinn AB|
|0x07C3|CIMTechniques, Inc.|
|0x07C4|Johnson Health Tech NA|
|0x07C5|June Life, Inc.|
|0x07C6|Bluenetics GmbH|
|0x07C7|iaconicDesign Inc.|
|0x07C8|WRLDS Creations AB|
|0x07C9|Skullcandy, Inc.|
|0x07CB|West Pharmaceutical Services, Inc.|
|0x07CC|Barnacle Systems Inc.|
|0x07CD|Smart Wave Technologies Canada Inc|
|0x07CE|Shanghai Top-Chip Microelectronics Tech. Co., LTD|
|0x07CF|NeoSensory, Inc.|
|0x07D0|Hangzhou Tuya Information Technology Co., Ltd|
|0x07D1|Shanghai Panchip Microelectronics Co., Ltd|
|0x07D2|React Accessibility Limited|
|0x07D3|LIVNEX Co.,Ltd.|
|0x07D4|Kano Computing Limited|
|0x07D5|hoots classic GmbH|
|0x07D6|ecobee Inc.|
|0x07D7|Nanjing Qinheng Microelectronics Co., Ltd|
|0x07D8|SOLUTIONS AMBRA INC.|
|0x07D9|Micro-Design, Inc.|
|0x07DA|STARLITE Co., Ltd.|
|0x07DB|Remedee Labs|
|0x07DC|ThingOS GmbH & Co KG|
|0x07DD|Linear Circuits|
|0x07DE|Unlimited Engineering SL|
|0x07DF|Snap-on Incorporated|
|0x07E0|Edifier International Limited|
|0x07E2|Alfred Kaercher SE & Co. KG|



Bluetooth SIG Proprietary 

Page 270 of 442 

**Assigned Numbers** / Document 

**2026-06-23** 

|0x07E3|Airoha Technology Corp.|
|---|---|
|0x07E4|Geeksme S.L.|
|0x07E5|Minut, Inc.|
|0x07E6|Waybeyond Limited|
|0x07E7|Komfort IQ, Inc.|
|0x07E8|Packetcraft, Inc.|
|0x07E9|Häfele GmbH & Co KG|
|0x07EA|ShapeLog, Inc.|
|0x07EB|NOVABASE S.R.L.|
|0x07EC|Frecce LLC|
|0x07ED|Joule IQ, INC.|
|0x07EE|KidzTek LLC|
|0x07EF|Aktiebolaget Sandvik Coromant|
|0x07F0|e-moola.com Pty Ltd|
|0x07F1|Zimi Innovations Pty Ltd|
|0x07F2|SERENE GROUP, INC|
|0x07F3|DIGISINE ENERGYTECH CO. LTD.|
|0x07F4|MEDIRLAB Orvosbiologiai Fejleszto Korlatolt Felelossegu Tarsasag|
|0x07F5|Byton North America Corporation|
|0x07F6|Shenzhen TonliScience and Technology Development Co.,Ltd|
|0x07F7|Cesar Systems Ltd.|
|0x07F8|quip NYC Inc.|
|0x07F9|Direct Communication Solutions, Inc.|
|0x07FA|Klipsch Group, Inc.|
|0x07FB|Access Co., Ltd|
|0x07FC|Renault SA|
|0x07FD|JSK CO., LTD.|
|0x07FE|BIROTA|
|0x07FF|maxon motor ltd.|
|0x0800|Optek|
|0x0801|CRONUS ELECTRONICS LTD|
|0x0802|NantSound, Inc.|
|0x0803|Domintell s.a.|
|0x0804|Andon Health Co.,Ltd|
|0x0805|Urbanminded Ltd|
|0x0806|TYRI Sweden AB|



Bluetooth SIG Proprietary 

Page 271 of 442 

**Assigned Numbers** / Document 

**2026-06-23** 

|0x0807|ECD Electronic Components GmbH Dresden|
|---|---|
|0x0808|SISTEMAS KERN, SOCIEDAD ANÓMINA|
|0x0809|Trulli Audio|
|0x080A|Altaneos|
|0x080B|Nanoleaf Canada Limited|
|0x080C|Ingy B.V.|
|0x080D|Azbil Co.|
|0x080E|TATTCOM LLC|
|0x080F|Paradox Engineering SA|
|0x0810|LECO Corporation|
|0x0811|Becker Antriebe GmbH|
|0x0812|Mstream Technologies., Inc.|
|0x0813|Flextronics International USA Inc.|
|0x0814|Ossur hf.|
|0x0815|SKC Inc|
|0x0816|SPICA SYSTEMS LLC|
|0x0817|Wangs Alliance Corporation|
|0x0818|tatwah SA|
|0x0819|Hunter Douglas Inc|
|0x081A|Shenzhen Conex|
|0x081B|DIM3|
|0x081C|Bobrick Washroom Equipment, Inc.|
|0x081D|Potrykus Holdings and Development LLC|
|0x081E|iNFORM Technology GmbH|
|0x081F|eSenseLab LTD|
|0x0820|Brilliant Home Technology, Inc.|
|0x0821|INOVA Geophysical, Inc.|
|0x0822|adafruit industries|
|0x0823|Nexite Ltd|
|0x0824|8Power Limited|
|0x0825|CME PTE. LTD.|
|0x0826|Hyundai Motor Company|
|0x0827|Kickmaker|
|0x0828|Shanghai Suisheng Information Technology Co., Ltd.|
|0x0829|HEXAGON METROLOGY DIVISION ROMER|
|0x082A|Mitutoyo Corporation|



Bluetooth SIG Proprietary 

Page 272 of 442 

**Assigned Numbers** / Document 

**2026-06-23** 

|0x082B|shenzhen fitcare electronics Co.,Ltd|
|---|---|
|0x082C|INGICS TECHNOLOGY CO., LTD.|
|0x082D|INCUS PERFORMANCE LTD.|
|0x082E|ABB S.p.A.|
|0x082F|Blippit AB|
|0x0831|Foxble, LLC|
|0x0832|Intermotive,Inc.|
|0x0833|Conneqtech B.V.|
|0x0834|RIKEN KEIKI CO., LTD.,|
|0x0835|Canopy Growth Corporation|
|0x0836|Bitwards Oy|
|0x0837|vivo Mobile Communication Co., Ltd.|
|0x0838|Etymotic Research, Inc.|
|0x0839|A puissance 3|
|0x083A|BPW Bergische Achsen Kommanditgesellschaft|
|0x083B|Piaggio Fast Forward|
|0x083C|BeerTech LTD|
|0x083D|Tokenize, Inc.|
|0x083E|Zorachka LTD|
|0x083F|D-Link Corp.|
|0x0840|Down Range Systems LLC|
|0x0841|General Luminaire (Shanghai) Co., Ltd.|
|0x0842|Tangshan HongJia electronic technology co., LTD.|
|0x0843|FRAGRANCE DELIVERY TECHNOLOGIES LTD|
|0x0844|Pepperl + Fuchs GmbH|
|0x0845|Dometic Corporation|
|0x0846|USound GmbH|
|0x0847|DNANUDGE LIMITED|
|0x0848|JUJU JOINTS CANADA CORP.|
|0x0849|Dopple Technologies B.V.|
|0x084A|ARCOM|
|0x084B|Biotechware SRL|
|0x084C|ORSO Inc.|
|0x084D|SafePort|
|0x084E|Carol Cole Company|
|0x084F|Embedded Fitness B.V.|



Bluetooth SIG Proprietary 

Page 273 of 442 

**Assigned Numbers** / Document 

**2026-06-23** 

|0x0850|Yealink (Xiamen) Network Technology Co.,LTD|
|---|---|
|0x0851|Subeca, Inc.|
|0x0852|Cognosos, Inc.|
|0x0853|Pektron Group Limited|
|0x0854|Tap Sound System|
|0x0855|Helios Sports, Inc.|
|0x0856|Canopy Growth Corporation|
|0x0857|Parsyl Inc|
|0x0858|SOUNDBOKS|
|0x0859|BlueUp|
|0x085A|DAKATECH|
|0x085B|Nisshinbo Micro Devices Inc.|
|0x085C|ACOS CO.,LTD.|
|0x085D|Guilin Zhishen Information Technology Co.,Ltd.|
|0x085E|Krog Systems LLC|
|0x0860|Alflex Products B.V.|
|0x0861|SmartSensor Labs Ltd|
|0x0862|SmartDrive|
|0x0863|Yo-tronics Technology Co., Ltd.|
|0x0864|Rafaelmicro|
|0x0865|Emergency Lighting Products Limited|
|0x0866|LAONZ Co.,Ltd|
|0x0867|Western Digital Techologies, Inc.|
|0x0868|WIOsense GmbH & Co. KG|
|0x0869|EVVA Sicherheitstechnologie GmbH|
|0x086A|Odic Incorporated|
|0x086B|Pacific Track, LLC|
|0x086C|Revvo Technologies, Inc.|
|0x086D|Biometrika d.o.o.|
|0x086E|Vorwerk Elektrowerke GmbH & Co. KG|
|0x086F|Trackunit A/S|
|0x0870|Wyze Labs, Inc|
|0x0871|Dension Elektronikai Kft.|
|0x0872|11 Health & Technologies Limited|
|0x0873|Innophase Incorporated|
|0x0874|Treegreen Limited|



Bluetooth SIG Proprietary 

Page 274 of 442 

**Assigned Numbers** / Document 

**2026-06-23** 

|0x0875|Berner International LLC|
|---|---|
|0x0876|SmartResQ ApS|
|0x0877|Valtech|
|0x0878|The Chamberlain Group, Inc.|
|0x0879|MIZUNO Corporation|
|0x087A|ZRF, LLC|
|0x087B|BYSTAMP|
|0x087C|Crosscan GmbH|
|0x087D|Konftel AB|
|0x087E|1bar.net Limited|
|0x087F|Phillips Connect Technologies LLC|
|0x0880|imagiLabs AB|
|0x0881|Optalert|
|0x0882|PSYONIC, Inc.|
|0x0883|Wintersteiger AG|
|0x0884|Controlid Industria, Comercio de Hardware e Servicos de Tecnologia Ltda|
|0x0886|Movella Technologies B.V.|
|0x0887|Hydro-Gear Limited Partnership|
|0x0888|EnPointe Fencing Pty Ltd|
|0x0889|XANTHIO|
|0x088A|sclak s.r.l.|
|0x088B|Tricorder Arraay Technologies LLC|
|0x088C|GB Solution co.,Ltd|
|0x088D|Soliton Systems K.K.|
|0x088F|Tait International Limited|
|0x0890|NICHIEI INTEC CO., LTD.|
|0x0891|SmartWireless GmbH & Co. KG|
|0x0892|Ingenieurbuero Birnfeld UG (haftungsbeschraenkt)|
|0x0893|Maytronics Ltd|
|0x0894|EPIFIT|
|0x0895|Gimer medical|
|0x0896|Nokian Renkaat Oyj|
|0x0897|Current Lighting Solutions LLC|
|0x0899|SFS unimarket AG|
|0x089A|Private limited company ”Teltonika”|
|0x089B<br>Bluetooth SI|Saucon Technologies<br>G Proprietary<br>Pag|



Page 275 of 442 

**Assigned Numbers** / Document 

**2026-06-23** 

|0x089C|Embedded Devices Co. Company|
|---|---|
|0x089D|J-J.A.D.E. Enterprise LLC|
|0x089E|i-SENS, inc.|
|0x089F|Witschi Electronic Ltd|
|0x08A0|Aclara Technologies LLC|
|0x08A1|EXEO TECH CORPORATION|
|0x08A2|Epic Systems Co., Ltd.|
|0x08A3|Hoffmann SE|
|0x08A4|Realme Chongqing Mobile Telecommunications Corp., Ltd.|
|0x08A6|Intelligenceworks Inc.|
|0x08A7|TGR 1.618 Limited|
|0x08A8|Shanghai Kfcube Inc|
|0x08A9|Fraunhofer IIS|
|0x08AA|SZ DJI TECHNOLOGY CO.,LTD|
|0x08AB|Coburn Technology, LLC|
|0x08AC|Topre Corporation|
|0x08AD|Kayamatics Limited|
|0x08AE|Moticon ReGo AG|
|0x08AF|Polidea Sp. z o.o.|
|0x08B0|Trivedi Advanced Technologies LLC|
|0x08B1|CORE|vision BV|
|0x08B2|PF SCHWEISSTECHNOLOGIE GMBH|
|0x08B3|IONIQ Skincare GmbH & Co. KG|
|0x08B4|Sengled Co., Ltd.|
|0x08B6|Boehringer Ingelheim Vetmedica GmbH|
|0x08B7|ABB Inc|
|0x08B8|Check Technology Solutions LLC|
|0x08B9|U-Shin Ltd.|
|0x08BA|HYPER ICE, INC.|
|0x08BB|Tokai-rika co.,ltd.|
|0x08BC|Prevayl Limited|
|0x08BD|bf1systems limited|
|0x08BE|ubisys technologies GmbH|
|0x08BF|SIRC Co., Ltd.|
|0x08C0|Accent Advanced Systems SLU|
|0x08C1|Rayden.Earth LTD|



Bluetooth SIG Proprietary 

Page 276 of 442 

**Assigned Numbers** / Document 

**2026-06-23** 

|0x08C2|Lindinvent AB|
|---|---|
|0x08C3|CHIPOLO d.o.o.|
|0x08C5|J. Wagner GmbH|
|0x08C7|Monadnock Systems Ltd.|
|0x08C8|Liteboxer Technologies Inc.|
|0x08C9|Noventa AG|
|0x08CA|Nubia Technology Co.,Ltd.|
|0x08CB|JT INNOVATIONS LIMITED|
|0x08CC|TGM TECHNOLOGY CO., LTD.|
|0x08CD|ifly|
|0x08CE|ZIMI CORPORATION|
|0x08CF|betternotstealmybike UG (with limited liability)|
|0x08D0|ESTOM Infotech Kft.|
|0x08D1|Sensovium Inc.|
|0x08D2|Virscient Limited|
|0x08D3|Novel Bits, LLC|
|0x08D4|ADATA Technology Co., LTD.|
|0x08D5|KEYes|
|0x08D7|Inovonics Corp|
|0x08D8|WARES|
|0x08D9|Pointr Labs Limited|
|0x08DA|Miridia Technology Incorporated|
|0x08DB|Tertium Technology|
|0x08DC|SHENZHEN AUKEY E BUSINESS CO., LTD|
|0x08DD|code-Q|
|0x08DE|TE Connectivity Corporation|
|0x08DF|IRIS OHYAMA CO.,LTD.|
|0x08E0|Philia Technology|
|0x08E1|KOZO KEIKAKU ENGINEERING Inc.|
|0x08E2|Shenzhen Simo Technology co. LTD|
|0x08E3|Republic Wireless, Inc.|
|0x08E4|Rashidov ltd|
|0x08E5|Crowd Connected Ltd|
|0x08E6|Eneso Tecnologia de Adaptacion S.L.|
|0x08E7|Barrot Technology Co.,Ltd.|
|0x08E8|Naonext|



Bluetooth SIG Proprietary 

Page 277 of 442 

**Assigned Numbers** / Document 

**2026-06-23** 

|0x08E9|Taiwan Intelligent Home Corp.|
|---|---|
|0x08EA|COWBELL ENGINEERING CO.,LTD.|
|0x08EB|Beijing Big Moment Technology Co., Ltd.|
|0x08EC|Denso Corporation|
|0x08ED|IMI Hydronic Engineering International SA|
|0x08EE|Askey Computer Corp.|
|0x08EF|Cumulus Digital Systems, Inc|
|0x08F0|Joovv, Inc.|
|0x08F1|The L.S. Starrett Company|
|0x08F2|Microoled|
|0x08F3|PSP - Pauli Services & Products GmbH|
|0x08F4|Kodimo Technologies Company Limited|
|0x08F5|Tymtix Technologies Private Limited|
|0x08F6|Dermal Photonics Corporation|
|0x08F7|MTD Products Inc & Affiliates|
|0x08F8|instagrid GmbH|
|0x08F9|Spacelabs Medical Inc.|
|0x08FB|Darkglass Electronics Oy|
|0x08FC|Hill-Rom|
|0x08FD|BioIntelliSense, Inc.|
|0x08FE|Ketronixs Sdn Bhd|
|0x08FF|Plastimold Products, Inc|
|0x0900|Beijing Zizai Technology Co., LTD.|
|0x0901|Lucimed|
|0x0902|TSC Auto-ID Technology Co., Ltd.|
|0x0903|DATAMARS, Inc.|
|0x0904|SUNCORPORATION|
|0x0905|Yandex Services AG|
|0x0906|Scope Logistical Solutions|
|0x0907|User Hello, LLC|
|0x0908|Pinpoint Innovations Limited|
|0x0909|70mai Co.,Ltd.|
|0x090A|Zhuhai Hoksi Technology CO.,LTD|
|0x090B|EMBR labs, INC|
|0x090C|Radiawave Technologies Co.,Ltd.|
|0x090D|IOT Invent GmbH|



Bluetooth SIG Proprietary 

Page 278 of 442 

**Assigned Numbers** / Document 

**2026-06-23** 

|0x090E|OPTIMUSIOT TECH LLP|
|---|---|
|0x090F|VC Inc.|
|0x0910|ASR Microelectronics (Shanghai) Co., Ltd.|
|0x0911|Douglas Lighting Controls Inc.|
|0x0912|Nerbio Medical Software Platforms Inc|
|0x0913|Braveheart Wireless, Inc.|
|0x0914|INEO-SENSE|
|0x0915|Honda Motor Co., Ltd.|
|0x0916|Ambient Sensors LLC|
|0x0917|ASR Microelectronics(ShenZhen)Co., Ltd.|
|0x0918|Technosphere Labs Pvt. Ltd.|
|0x0919|NO SMD LIMITED|
|0x091A|Albertronic BV|
|0x091B|Luminostics, Inc.|
|0x091C|Oblamatik AG|
|0x091D|Innokind, Inc.|
|0x091E|Melbot Studios, Sociedad Limitada|
|0x091F|Myzee Technology|
|0x0921|KAHA PTE. LTD.|
|0x0922|Shanghai MXCHIP Information Technology Co., Ltd.|
|0x0923|JSB TECH PTE LTD|
|0x0924|Fundacion Tecnalia Research and Innovation|
|0x0925|Yukai Engineering Inc.|
|0x0926|Gooligum Technologies Pty Ltd|
|0x0927|ROOQ GmbH|
|0x0928|AiRISTA|
|0x0929|Qingdao Haier Technology Co., Ltd.|
|0x092A|Sappl Verwaltungs- und Betriebs GmbH|
|0x092B|TekHome|
|0x092C|PCI Private Limited|
|0x092D|Leggett & Platt, Incorporated|
|0x092E|PS GmbH|
|0x092F|C.O.B.O. SpA|
|0x0930|James Walker RotaBolt Limited|
|0x0931|BREATHINGS Co., Ltd.|
|0x0933|SRAM|



Bluetooth SIG Proprietary 

Page 279 of 442 

**Assigned Numbers** / Document 

**2026-06-23** 

|0x0934|KiteSpring Inc.|
|---|---|
|0x0935|Reconnect, Inc.|
|0x0936|Elekon AG|
|0x0937|RealThingks GmbH|
|0x0938|Henway Technologies, LTD.|
|0x0939|ASTEM Co.,Ltd.|
|0x093A|LinkedSemi Microelectronics (Xiamen) Co., Ltd|
|0x093B|ENSESO LLC|
|0x093C|Xenoma Inc.|
|0x093D|Adolf Wuerth GmbH & Co KG|
|0x093E|Catalyft Labs, Inc.|
|0x093F|JEPICO Corporation|
|0x0940|Hero Workout GmbH|
|0x0941|Rivian Automotive, LLC|
|0x0942|TRANSSION HOLDINGS LIMITED|
|0x0944|Agitron d.o.o.|
|0x0945|Globe (Jiangsu) Co., Ltd|
|0x0946|AMC International Alfa Metalcraft Corporation AG|
|0x0947|First Light Technologies Ltd.|
|0x0948|Wearable Link Limited|
|0x0949|Metronom Health Europe|
|0x094A|Zwift, Inc.|
|0x094B|Kindeva Drug Delivery L.P.|
|0x094C|GimmiSys GmbH|
|0x094D|tkLABS INC.|
|0x094E|PassiveBolt, Inc.|
|0x094F|Limited Liability Company ”Mikrotikls”|
|0x0950|Capetech|
|0x0951|PPRS|
|0x0952|Apptricity Corporation|
|0x0953|LogiLube, LLC|
|0x0954|Julbo|
|0x0955|Breville Group|
|0x0956|Kerlink|
|0x0957|Ohsung Electronics|
|0x0958|ZTE Corporation|



Bluetooth SIG Proprietary 

Page 280 of 442 

**Assigned Numbers** / Document 

**2026-06-23** 

|0x0959|HerdDogg, Inc|
|---|---|
|0x095B|Lismore Instruments Limited|
|0x095C|LogiLube, LLC|
|0x095D|Electronic Theatre Controls|
|0x095E|BioEchoNet inc.|
|0x095F|NUANCE HEARING LTD|
|0x0960|Sena Technologies Inc.|
|0x0961|Linkura AB|
|0x0962|GL Solutions K.K.|
|0x0963|Moonbird BV|
|0x0964|Countrymate Technology Limited|
|0x0965|Asahi Kasei Corporation|
|0x0966|PointGuard, LLC|
|0x0967|Neo Materials and Consulting Inc.|
|0x0968|Actev Motors, Inc.|
|0x0969|Woan Technology (Shenzhen) Co., Ltd.|
|0x096A|dricos, Inc.|
|0x096B|Guide ID B.V.|
|0x096D|Gunwerks, LLC|
|0x096E|Band Industries, inc.|
|0x0970|IBA Dosimetry GmbH|
|0x0971|GA|
|0x0973|Popit Oy|
|0x0974|ABEYE|
|0x0975|BlueIOT(Beijing) Technology Co.,Ltd|
|0x0976|Fauna Audio GmbH|
|0x0977|TOYOTA motor corporation|
|0x0978|ZifferEins GmbH & Co. KG|
|0x0979|BIOTRONIK SE & Co. KG|
|0x097A|CORE CORPORATION|
|0x097B|CTEK Sweden AB|
|0x097C|Thorley Industries, LLC|
|0x097D|CLB B.V.|
|0x097E|SonicSensory Inc|
|0x097F|ISEMAR S.R.L.|
|0x0980|DEKRA TESTING AND CERTIFICATION, S.A.U.|



Bluetooth SIG Proprietary 

Page 281 of 442 

**Assigned Numbers** / Document 

**2026-06-23** 

|0x0981|Bernard Krone Holding SE & Co.KG|
|---|---|
|0x0982|ELPRO-BUCHS AG|
|0x0983|Feedback Sports LLC|
|0x0984|TeraTron GmbH|
|0x0985|Lumos Health Inc.|
|0x0986|Cello Hill, LLC|
|0x0987|TSE BRAKES, INC.|
|0x0988|BHM-Tech Produktionsgesellschaft m.b.H|
|0x0989|WIKA Alexander Wiegand SE & Co.KG|
|0x098A|Biovigil|
|0x098B|Mequonic Engineering, S.L.|
|0x098C|bGrid B.V.|
|0x098E|ADVEEZ|
|0x098F|Aktiebolaget Regin|
|0x0990|Anton Paar GmbH|
|0x0991|Telenor ASA|
|0x0992|Big Kaiser Precision Tooling Ltd|
|0x0993|Absolute Audio Labs B.V.|
|0x0994|VT42 Pty Ltd|
|0x0995|Bronkhorst High-Tech B.V.|
|0x0996|C. & E. Fein GmbH|
|0x0997|NextMind|
|0x0998|Pixie Dust Technologies, Inc.|
|0x0999|eTactica ehf|
|0x099A|New Audio LLC|
|0x099B|Sendum Wireless Corporation|
|0x099C|deister electronic GmbH|
|0x099D|YKK AP Inc.|
|0x099E|Step One Limited|
|0x099F|Koya Medical, Inc.|
|0x09A0|Proof Diagnostics, Inc.|
|0x09A1|VOS Systems, LLC|
|0x09A2|ENGAGENOW DATA SCIENCES PRIVATE LIMITED|
|0x09A3|ARDUINO SA|
|0x09A4|KUMHO ELECTRICS, INC|
|0x09A5|Security Enhancement Systems, LLC|



Bluetooth SIG Proprietary 

Page 282 of 442 

**Assigned Numbers** / Document 

**2026-06-23** 

|0x09A6|BEIJING ELECTRIC VEHICLE CO.,LTD|
|---|---|
|0x09A7|Paybuddy ApS|
|0x09A8|KHN Solutions LLC|
|0x09A9|Nippon Ceramic Co.,Ltd.|
|0x09AA|PHOTODYNAMIC INCORPORATED|
|0x09AB|DashLogic, Inc.|
|0x09AC|Ambiq|
|0x09AD|Narhwall Inc.|
|0x09AE|Pozyx NV|
|0x09AF|ifLink Open Community|
|0x09B0|Deublin Company, LLC|
|0x09B1|BLINQY|
|0x09B2|DYPHI|
|0x09B3|BlueX Microelectronics Corp Ltd.|
|0x09B4|PentaLock Aps.|
|0x09B5|AUTEC Gesellschaft fuer Automationstechnik mbH|
|0x09B6|Pegasus Technologies, Inc.|
|0x09B7|Bout Labs, LLC|
|0x09B8|PlayerData Limited|
|0x09B9|SAVOY ELECTRONIC LIGHTING|
|0x09BA|Elimo Engineering Ltd|
|0x09BB|SkyStream Corporation|
|0x09BC|Aerosens LLC|
|0x09BD|Centre Suisse d’Electronique et de Microtechnique SA|
|0x09BE|Vessel Ltd.|
|0x09BF|Span.IO, Inc.|
|0x09C0|AnotherBrain inc.|
|0x09C1|Rosewill|
|0x09C2|Universal Audio, Inc.|
|0x09C3|JAPAN TOBACCO INC.|
|0x09C4|UVISIO|
|0x09C5|HungYi Microelectronics Co.,Ltd.|
|0x09C6|Honor Device Co., Ltd.|
|0x09C7|Combustion, LLC|
|0x09C8|XUNTONG|
|0x09C9|CrowdGlow Ltd|



Bluetooth SIG Proprietary 

Page 283 of 442 

**Assigned Numbers** / Document 

**2026-06-23** 

|0x09CA|Mobitrace|
|---|---|
|0x09CB|Hx Engineering, LLC|
|0x09CC|Senso4s d.o.o.|
|0x09CE|Julius Blum GmbH|
|0x09CF|BlueStreak IoT, LLC|
|0x09D0|Chess Wise B.V.|
|0x09D1|ABLEPAY TECHNOLOGIES AS|
|0x09D2|Temperature Sensitive Solutions Systems Sweden AB|
|0x09D4|ORBIS Inc.|
|0x09D5|GEAR RADIO ELECTRONICS CORP.|
|0x09D6|EAR TEKNIK ISITME VE ODIOMETRI CIHAZLARI SANAYI VE TICARET<br>ANONIM SIRKETI|
|0x09D7|Coyotta|
|0x09D8|Synergy Tecnologia em Sistemas Ltda|
|0x09D9|VivoSensMedical GmbH|
|0x09DA|Nagravision SA|
|0x09DB|Bionic Avionics Inc.|
|0x09DD|Innoware Development AB|
|0x09DE|JLD Technology Solutions, LLC|
|0x09DF|Magnus Technology Sdn Bhd|
|0x09E1|Tag-N-Trac Inc|
|0x09E3|Friday Home Aps|
|0x09E4|CPS AS|
|0x09E5|Mobilogix|
|0x09E6|Masonite Corporation|
|0x09E7|Kabushikigaisha HANERON|
|0x09E8|Melange Systems Pvt. Ltd.|
|0x09E9|LumenRadio AB|
|0x09EA|Athlos Oy|
|0x09EB|KEAN ELECTRONICS PTY LTD|
|0x09EC|Yukon advanced optics worldwide, UAB|
|0x09ED|Sibel Inc.|
|0x09EE|OJMAR SA|
|0x09EF|Steinel Solutions AG|
|0x09F0|WatchGas B.V.|
|0x09F1|OM Digital Solutions Corporation|



Bluetooth SIG Proprietary 

Page 284 of 442 

**Assigned Numbers** / Document 

**2026-06-23** 

|0x09F2|Audeara Pty Ltd|
|---|---|
|0x09F3|Beijing Zero Zero Infinity Technology Co.,Ltd.|
|0x09F4|Spectrum Technologies, Inc.|
|0x09F5|OKI Electric Industry Co., Ltd|
|0x09F6|Mobile Action Technology Inc.|
|0x09F7|SENSATEC Co., Ltd.|
|0x09F8|R.O. S.R.L.|
|0x09F9|Hangzhou Yaguan Technology Co. LTD|
|0x09FA|Listen Technologies Corporation|
|0x09FB|TOITU CO., LTD.|
|0x09FC|Confidex|
|0x09FE|Lichtvision Engineering GmbH|
|0x09FF|AIRSTAR|
|0x0A00|Ampler Bikes OU|
|0x0A01|Cleveron AS|
|0x0A02|Ayxon-Dynamics GmbH|
|0x0A03|donutrobotics Co., Ltd.|
|0x0A04|Flosonics Medical|
|0x0A05|Southwire Company, LLC|
|0x0A06|Shanghai wuqi microelectronics Co.,Ltd|
|0x0A07|Reflow Pty Ltd|
|0x0A08|Oras Oy|
|0x0A0A|Volan Technology Inc.|
|0x0A0B|SIANA Systems|
|0x0A0C|Shanghai Yidian Intelligent Technology Co., Ltd.|
|0x0A0D|Blue Peacock GmbH|
|0x0A0E|Roland Corporation|
|0x0A0F|LIXIL Corporation|
|0x0A10|SUBARU Corporation|
|0x0A11|Sensolus|
|0x0A12|Dyson Technology Limited|
|0x0A13|Tec4med LifeScience GmbH|
|0x0A14|CROXEL, INC.|
|0x0A15|Syng Inc|
|0x0A16|RIDE VISION LTD|
|0x0A17|Plume Design Inc|



Bluetooth SIG Proprietary 

Page 285 of 442 

**Assigned Numbers** / Document 

**2026-06-23** 

|0x0A18|Cambridge Animal Technologies Ltd|
|---|---|
|0x0A19|Maxell, Ltd.|
|0x0A1A|Link Labs, Inc.|
|0x0A1B|Embrava Pty Ltd|
|0x0A1C|INPEAK S.C.|
|0x0A1D|API-K|
|0x0A1E|CombiQ AB|
|0x0A1F|DeVilbiss Healthcare LLC|
|0x0A20|Jiangxi Innotech Technology Co., Ltd|
|0x0A21|Apollogic Sp. z o.o.|
|0x0A22|DAIICHIKOSHO CO., LTD.|
|0x0A23|BIXOLON CO.,LTD|
|0x0A24|Atmosic Technologies, Inc.|
|0x0A25|Eran Financial Services LLC|
|0x0A26|Louis Vuitton|
|0x0A28|NanoFlex Power Corporation|
|0x0A29|Worthcloud Technology Co.,Ltd|
|0x0A2A|Yamaha Corporation|
|0x0A2B|PaceBait IVS|
|0x0A2C|Shenzhen H&T Intelligent Control Co., Ltd|
|0x0A2D|Shenzhen Feasycom Technology Co., Ltd.|
|0x0A2F|Instamic, Inc.|
|0x0A30|Air-Weigh|
|0x0A31|Nevro Corp.|
|0x0A32|Pinnacle Technology, Inc.|
|0x0A33|WMF AG|
|0x0A34|Luxer Corporation|
|0x0A35|safectory GmbH|
|0x0A36|NGK SPARK PLUG CO., LTD.|
|0x0A37|2587702 Ontario Inc.|
|0x0A38|Bouffalo Lab (Nanjing)., Ltd.|
|0x0A39|BLUETICKETING SRL|
|0x0A3A|Incotex Co. Ltd.|
|0x0A3B|Galileo Technology Limited|
|0x0A3C|Siteco GmbH|
|0x0A3D|DELABIE|



Bluetooth SIG Proprietary 

Page 286 of 442 

**Assigned Numbers** / Document 

**2026-06-23** 

|0x0A3F|Shenzhen Yopeak Optoelectronics Technology Co., Ltd.|
|---|---|
|0x0A41|OPEX Corporation|
|0x0A42|Motionalysis, Inc.|
|0x0A43|Busch Systems International Inc.|
|0x0A44|Novidan, Inc.|
|0x0A45|3SI Security Systems, Inc|
|0x0A46|Beijing HC-Infinite Technology Limited|
|0x0A47|The Wand Company Ltd|
|0x0A48|JRC Mobility Inc.|
|0x0A4A|Map Large, Inc.|
|0x0A4B|MistyWest Energy and Transport Ltd.|
|0x0A4C|SiFli Technologies (shanghai) Inc.|
|0x0A4D|Lockn Technologies Private Limited|
|0x0A4E|Toytec Corporation|
|0x0A4F|VANMOOF Global Holding B.V.|
|0x0A50|Nextscape Inc.|
|0x0A51|CSIRO|
|0x0A52|Follow Sense Europe B.V.|
|0x0A53|KKM COMPANY LIMITED|
|0x0A54|SQL Technologies Corp.|
|0x0A55|Inugo Systems Limited|
|0x0A56|ambie|
|0x0A57|Meizhou Guo Wei Electronics Co., Ltd|
|0x0A58|Indigo Diabetes|
|0x0A59|TourBuilt, LLC|
|0x0A5A|Sontheim Industrie Elektronik GmbH|
|0x0A5B|LEGIC Identsystems AG|
|0x0A5C|Innovative Design Labs Inc.|
|0x0A5D|MG Energy Systems B.V.|
|0x0A5F|stryker|
|0x0A60|DATANG SEMICONDUCTOR TECHNOLOGY CO.,LTD|
|0x0A61|Smart Parks B.V.|
|0x0A62|MOKO TECHNOLOGY Ltd|
|0x0A64|Geopal system A/S|
|0x0A65|Lytx, INC.|
|0x0A67|Beijing SuperHexa Century Technology CO. Ltd|



Bluetooth SIG Proprietary 

Page 287 of 442 

**Assigned Numbers** / Document 

**2026-06-23** 

|0x0A68|Focus Ingenieria SRL|
|---|---|
|0x0A69|HAPPIEST BABY, INC.|
|0x0A6A|Scribble Design Inc.|
|0x0A6B|Olympic Ophthalmics, Inc.|
|0x0A6C|Pokkels|
|0x0A6D|KUUKANJYOKIN Co.,Ltd.|
|0x0A6E|Pac Sane Limited|
|0x0A6F|Warner Bros.|
|0x0A70|Ooma|
|0x0A71|Senquip Pty Ltd|
|0x0A72|Jumo GmbH & Co. KG|
|0x0A73|Innohome Oy|
|0x0A74|MICROSON S.A.|
|0x0A75|Delta Cycle Corporation|
|0x0A76|Synaptics Incorporated|
|0x0A77|AXTRO PTE. LTD.|
|0x0A78|Shenzhen Sunricher Technology Limited|
|0x0A79|Webasto SE|
|0x0A7A|Emlid Limited|
|0x0A7B|UniqAir Oy|
|0x0A7C|WAFERLOCK|
|0x0A7D|Freedman Electronics Pty Ltd|
|0x0A7E|KEBA Handover Automation GmbH|
|0x0A7F|Intuity Medical|
|0x0A80|Cleer Limited|
|0x0A81|Universal Biosensors Pty Ltd|
|0x0A82|Corsair|
|0x0A83|Rivata, Inc.|
|0x0A84|Greennote Inc,|
|0x0A85|Snowball Technology Co., Ltd.|
|0x0A86|ALIZENT International|
|0x0A87|Shanghai Smart System Technology Co., Ltd|
|0x0A88|PSA Peugeot Citroen|
|0x0A89|VusionGroup|
|0x0A8A|HAINBUCH GMBH SPANNENDE TECHNIK|
|0x0A8B|SANlight GmbH|



Bluetooth SIG Proprietary 

Page 288 of 442 

**Assigned Numbers** / Document 

**2026-06-23** 

|0x0A8C|DelpSys, s.r.o.|
|---|---|
|0x0A8D|JCM TECHNOLOGIES S.A.|
|0x0A8E|Perfect Company|
|0x0A8F|TOTO LTD.|
|0x0A90|Shenzhen Grandsun Electronic Co.,Ltd.|
|0x0A91|Monarch International Inc.|
|0x0A92|Carestream Dental LLC|
|0x0A93|GiPStech S.r.l.|
|0x0A94|OOBIK Inc.|
|0x0A95|Pamex Inc.|
|0x0A98|Foil, Inc.|
|0x0A99|Shanghai high-flying electronics technology Co.,Ltd|
|0x0A9A|TEMKIN ASSOCIATES, LLC|
|0x0A9B|Eello LLC|
|0x0A9C|Xi’an Fengyu Information Technology Co., Ltd.|
|0x0A9D|Canon Finetech Nisca Inc.|
|0x0A9F|ista International GmbH|
|0x0AA0|Loy Tec electronics GmbH|
|0x0AA1|LINCOGN TECHNOLOGY CO. LIMITED|
|0x0AA2|Care Bloom, LLC|
|0x0AA3|DIC Corporation|
|0x0AA4|FAZEPRO LLC|
|0x0AA5|Shenzhen Uascent Technology Co., Ltd|
|0x0AA6|Realityworks, inc.|
|0x0AA7|Urbanista AB|
|0x0AA8|Zencontrol Pty Ltd|
|0x0AA9|Spintly, Inc.|
|0x0AAA|Computime International Ltd|
|0x0AAB|Anhui Listenai Co|
|0x0AAC|OSM HK Limited|
|0x0AAD|Adevo Consulting AB|
|0x0AAE|PS Engineering, Inc.|
|0x0AAF|AIAIAI ApS|
|0x0AB0|Visiontronic s.r.o.|
|0x0AB1|InVue Security Products Inc|
|0x0AB2|TouchTronics, Inc.|



Bluetooth SIG Proprietary 

Page 289 of 442 

**Assigned Numbers** / Document 

**2026-06-23** 

|0x0AB3|INNER RANGE PTY. LTD.|
|---|---|
|0x0AB4|Ellenby Technologies, Inc.|
|0x0AB5|Elstat Electronics Ltd.|
|0x0AB6|Xenter, Inc.|
|0x0AB7|LogTag North America Inc.|
|0x0AB8|Sens.ai Incorporated|
|0x0AB9|STL|
|0x0ABA|Open Bionics Ltd.|
|0x0ABB|R-DAS, s.r.o.|
|0x0ABC|KCCS Mobile Engineering Co., Ltd.|
|0x0ABD|Inventas AS|
|0x0ABE|Robkoo Information & Technologies Co., Ltd.|
|0x0ABF|PAUL HARTMANN AG|
|0x0AC0|Omni-ID USA, INC.|
|0x0AC1|Shenzhen Jingxun Technology Co., Ltd.|
|0x0AC2|RealMega Microelectronics technology (Shanghai) Co. Ltd.|
|0x0AC3|Kenzen, Inc.|
|0x0AC4|CODIUM|
|0x0AC5|Flexoptix GmbH|
|0x0AC6|Barnes Group Inc.|
|0x0AC7|Chengdu Aich Technology Co.,Ltd|
|0x0AC8|Keepin Co., Ltd.|
|0x0AC9|Swedlock AB|
|0x0ACA|Shenzhen CoolKit Technology Co., Ltd|
|0x0ACB|ise Individuelle Software und Elektronik GmbH|
|0x0ACC|Nuvoton|
|0x0ACD|Visuallex Sport International Limited|
|0x0ACE|KOBATA GAUGE MFG. CO., LTD.|
|0x0ACF|CACI Technologies|
|0x0AD0|Nordic Strong ApS|
|0x0AD1|EAGLE KINGDOM TECHNOLOGIES LIMITED|
|0x0AD2|Lautsprecher Teufel GmbH|
|0x0AD3|SSV Software Systems GmbH|
|0x0AD4|Zhuhai Pantum Electronisc Co., Ltd|
|0x0AD5|Streamit B.V.|
|0x0AD6|nymea GmbH|



Bluetooth SIG Proprietary 

Page 290 of 442 

**Assigned Numbers** / Document 

**2026-06-23** 

|0x0AD7|AL-KO Geraete GmbH|
|---|---|
|0x0AD8|Franz Kaldewei GmbH&Co KG|
|0x0ADA|Codefabrik GmbH|
|0x0ADB|Reelables, Inc.|
|0x0ADC|Duravit AG|
|0x0ADD|Boss Audio|
|0x0ADE|Vocera Communications, Inc.|
|0x0ADF|Douglas Dynamics L.L.C.|
|0x0AE3|GlobalMed|
|0x0AE4|DALI Alliance|
|0x0AE5|unu GmbH|
|0x0AE6|Hexology|
|0x0AE7|Sunplus Technology Co., Ltd.|
|0x0AE8|LEVEL, s.r.o.|
|0x0AE9|FLIR Systems AB|
|0x0AEA|Borda Technology|
|0x0AEB|Square, Inc.|
|0x0AEC|FUTEK ADVANCED SENSOR TECHNOLOGY, INC|
|0x0AED|Saxonar GmbH|
|0x0AEE|Velentium, LLC|
|0x0AEF|GLP German Light Products GmbH|
|0x0AF0|Leupold & Stevens, Inc.|
|0x0AF1|CRADERS,CO.,LTD|
|0x0AF2|Shanghai All Link Microelectronics Co.,Ltd|
|0x0AF3|701x Inc.|
|0x0AF4|Radioworks Microelectronics PTY LTD|
|0x0AF5|Unitech Electronic Inc.|
|0x0AF6|AMETEK, Inc.|
|0x0AF7|Irdeto|
|0x0AF8|First Design System Inc.|
|0x0AF9|Unisto AG|
|0x0AFA|Chengdu Ambit Technology Co., Ltd.|
|0x0AFB|SMT ELEKTRONIK GmbH|
|0x0AFC|Cerebrum Sensor Technologies Inc.|
|0x0AFD|Weber Sensors, LLC|
|0x0AFE|Earda Technologies Co.,Ltd|



Bluetooth SIG Proprietary 

Page 291 of 442 

**Assigned Numbers** / Document 

**2026-06-23** 

|0x0AFF|FUSEAWARE LIMITED|
|---|---|
|0x0B00|Flaircomm Microelectronics Inc.|
|0x0B01|RESIDEO TECHNOLOGIES, INC.|
|0x0B02|IORA Technology Development Ltd. Sti.|
|0x0B03|Precision Triathlon Systems Limited|
|0x0B05|Marquardt GmbH|
|0x0B06|FAZUA GmbH|
|0x0B07|Workaround Gmbh|
|0x0B08|Shenzhen Qianfenyi Intelligent Technology Co., LTD|
|0x0B0A|Belun Technology Company Limited|
|0x0B0B|Sanistaal A/S|
|0x0B0C|BluPeak|
|0x0B0D|SANYO DENKO Co.,Ltd.|
|0x0B0E|Minebea Access Solutions Inc.|
|0x0B0F|B.E.A. S.A.|
|0x0B10|Alfa Laval Corporate AB|
|0x0B11|ThermoWorks, Inc.|
|0x0B12|ToughBuilt Industries LLC|
|0x0B13|IOTOOLS|
|0x0B14|Olumee|
|0x0B15|NAOS JAPAN K.K.|
|0x0B16|Guard RFID Solutions Inc.|
|0x0B17|SIG SAUER, INC.|
|0x0B18|DECATHLON SE|
|0x0B19|WBS PROJECT H PTY LTD|
|0x0B1A|Roca Sanitario, S.A.|
|0x0B1C|Nanoleq AG|
|0x0B1D|Accelerated Systems|
|0x0B1E|PB INC.|
|0x0B1F|Beijing ESWIN Computing Technology Co., Ltd.|
|0x0B20|TKH Security B.V.|
|0x0B22|Hygiene IQ, LLC.|
|0x0B23|iRhythm Technologies, Inc.|
|0x0B24|BeiJing ZiJie TiaoDong KeJi Co.,Ltd.|
|0x0B25|NIBROTECH LTD|
|0x0B26|Baracoda Daily Healthtech.|



Bluetooth SIG Proprietary 

Page 292 of 442 

**Assigned Numbers** / Document 

**2026-06-23** 

|0x0B27|Lumi United Technology Co., Ltd|
|---|---|
|0x0B29|Tech-Venom Entertainment Private Limited|
|0x0B2B|MAINBOT|
|0x0B2C|ILLUMAGEAR, Inc.|
|0x0B2D|REDARC ELECTRONICS PTY LTD|
|0x0B2E|MOCA System Inc.|
|0x0B2F|Duke Manufacturing Co|
|0x0B30|ART SPA|
|0x0B31|Silver Wolf Vehicles Inc.|
|0x0B32|Hala Systems, Inc.|
|0x0B33|ARMATURA LLC|
|0x0B34|CONZUMEX INDUSTRIES PRIVATE LIMITED|
|0x0B35|BH SENS|
|0x0B36|SINTEF|
|0x0B37|Omnivoltaic Energy Solutions Limited Company|
|0x0B38|WISYCOM S.R.L.|
|0x0B39|Red 100 Lighting Co., ltd.|
|0x0B3A|Impact Biosystems, Inc.|
|0x0B3B|AIC semiconductor (Shanghai) Co., Ltd.|
|0x0B3C|Dodge Industrial, Inc.|
|0x0B3D|REALTIMEID AS|
|0x0B3E|ISEO Serrature S.p.a.|
|0x0B3F|MindRhythm, Inc.|
|0x0B40|Havells India Limited|
|0x0B41|Sentrax GmbH|
|0x0B42|TSI|
|0x0B43|INCITAT ENVIRONNEMENT|
|0x0B44|nFore Technology Co., Ltd.|
|0x0B45|Electronic Sensors, Inc.|
|0x0B47|Gentex Corporation|
|0x0B48|NIO USA, Inc.|
|0x0B49|SkyHawke Technologies|
|0x0B4A|Nomono AS|
|0x0B4B|EMS Integrators, LLC|
|0x0B4C|BiosBob.Biz|
|0x0B4D|Adam Hall GmbH|



Bluetooth SIG Proprietary 

Page 293 of 442 

**Assigned Numbers** / Document 

**2026-06-23** 

|0x0B4E|ICP Systems B.V.|
|---|---|
|0x0B4F|Breezi.io, Inc.|
|0x0B50|Mesh Systems LLC|
|0x0B51|FUN FACTORY GmbH|
|0x0B52|ZIIP Inc|
|0x0B53|SHENZHEN KAADAS INTELLIGENT TECHNOLOGY CO.,Ltd|
|0x0B54|Emotion Fitness GmbH & Co. KG|
|0x0B55|H G M Automotive Electronics, Inc.|
|0x0B56|BORA - Vertriebs GmbH & Co KG|
|0x0B57|CONVERTRONIX TECHNOLOGIES AND SERVICES LLP|
|0x0B58|TOKAI-DENSHI INC|
|0x0B5B|Shenzhen ImagineVision Technology Limited|
|0x0B5D|Fujian Newland Auto-ID Tech. Co., Ltd.|
|0x0B5E|CELLCONTROL, INC.|
|0x0B5F|Rivieh, Inc.|
|0x0B60|RATOC Systems, Inc.|
|0x0B61|Sentek Pty Ltd|
|0x0B62|NOVEA ENERGIES|
|0x0B63|Innolux Corporation|
|0x0B64|NingBo klite Electric Manufacture Co.,LTD|
|0x0B65|The Apache Software Foundation|
|0x0B66|MITSUBISHI ELECTRIC AUTOMATION (THAILAND) COMPANY LIMITED|
|0x0B67|CleanSpace Technology Pty Ltd|
|0x0B68|Quha oy|
|0x0B69|Addaday|
|0x0B6A|Dymo|
|0x0B6B|Samsara Networks, Inc|
|0x0B6C|Sensitech, Inc.|
|0x0B6D|SOLUM CO., LTD|
|0x0B6E|React Mobile|
|0x0B70|JDRF Electromag Engineering Inc|
|0x0B71|lilbit ODM AS|
|0x0B72|Geeknet, Inc.|
|0x0B73|HARADA INDUSTRY CO., LTD.|
|0x0B74|BQN|
|0x0B75<br>Bluetooth SI|Triple W Japan Inc.<br>G Proprietary<br>Page|



Page 294 of 442 

**Assigned Numbers** / Document 

**2026-06-23** 

|0x0B76|MAX-co., ltd|
|---|---|
|0x0B77|Aixlink(Chengdu) Co., Ltd.|
|0x0B78|FIELD DESIGN INC.|
|0x0B79|Sankyo Air Tech Co.,Ltd.|
|0x0B7A|Shenzhen KTC Technology Co.,Ltd.|
|0x0B7B|Hardcoder Oy|
|0x0B7C|Scangrip A/S|
|0x0B7D|FoundersLane GmbH|
|0x0B7E|Offcode Oy|
|0x0B7F|ICU tech GmbH|
|0x0B80|AXELIFE|
|0x0B81|SCM Group|
|0x0B82|Mammut Sports Group AG|
|0x0B83|Taiga Motors Inc.|
|0x0B84|Presidio Medical, Inc.|
|0x0B85|VIMANA TECH PTY LTD|
|0x0B86|Trek Bicycle|
|0x0B87|Ampetronic Ltd|
|0x0B88|Muguang (Guangdong) Intelligent Lighting Technology Co., Ltd|
|0x0B89|Rotronic AG|
|0x0B8A|Seiko Instruments Inc.|
|0x0B8B|American Technology Components, Incorporated|
|0x0B8C|MOTREX|
|0x0B8D|Pertech Industries Inc|
|0x0B8E|Gentle Energy Corp.|
|0x0B8F|Senscomm Semiconductor Co., Ltd.|
|0x0B91|Alfen ICU B.V.|
|0x0B93|Hangzhou BroadLink Technology Co., Ltd.|
|0x0B94|Dreem SAS|
|0x0B96|Telecom Design|
|0x0B97|SILVER TREE LABS, INC.|
|0x0B98|Gymstory B.V.|
|0x0B99|The Goodyear Tire & Rubber Company|
|0x0B9A|Beijing Wisepool Infinite Intelligence Technology Co.,Ltd|
|0x0B9C|Komatsu Ltd.|
|0x0B9D|Sensoria Holdings LTD|



Bluetooth SIG Proprietary 

Page 295 of 442 

**Assigned Numbers** / Document 

**2026-06-23** 

|0x0B9E|Audio Partnership Plc|
|---|---|
|0x0B9F|Group Lotus Limited|
|0x0BA0|Data Sciences International|
|0x0BA1|Bunn-O-Matic Corporation|
|0x0BA2|TireCheck GmbH|
|0x0BA3|Sonova Consumer Hearing GmbH|
|0x0BA4|Vervent Audio Group|
|0x0BA5|SONICOS ENTERPRISES, LLC|
|0x0BA6|Nissan Motor Co., Ltd.|
|0x0BA7|hearX Group (Pty) Ltd|
|0x0BA8|GLOWFORGE INC.|
|0x0BA9|Allterco Robotics ltd|
|0x0BAA|Infinitegra, Inc.|
|0x0BAB|Grandex International Corporation|
|0x0BAC|Machfu Inc.|
|0x0BAD|Roambotics, Inc.|
|0x0BAE|Soma Labs LLC|
|0x0BAF|NITTO KOGYO CORPORATION|
|0x0BB0|Ecolab Inc.|
|0x0BB1|Beijing ranxin intelligence technology Co.,LTD|
|0x0BB2|Fjorden Electra AS|
|0x0BB3|Flender GmbH|
|0x0BB4|New Cosmos USA, Inc.|
|0x0BB5|Xirgo Technologies, LLC|
|0x0BB6|Build With Robots Inc.|
|0x0BB7|IONA Tech LLC|
|0x0BB8|INNOVAG PTY. LTD.|
|0x0BB9|SaluStim Group Oy|
|0x0BBA|Huso, INC|
|0x0BBB|SWISSINNO SOLUTIONS AG|
|0x0BBC|T2REALITY SOLUTIONS PRIVATE LIMITED|
|0x0BBE|SAAB Aktiebolag|
|0x0BBF|HIMSA II K/S|
|0x0BC0|READY FOR SKY LLP|
|0x0BC1|Miele & Cie. KG|
|0x0BC2|EntWick Co.|



Bluetooth SIG Proprietary 

Page 296 of 442 

**Assigned Numbers** / Document 

**2026-06-23** 

|0x0BC3|MCOT INC.|
|---|---|
|0x0BC4|TECHTICS ENGINEERING B.V.|
|0x0BC5|Aperia Technologies, Inc.|
|0x0BC6|TCL COMMUNICATION EQUIPMENT CO.,LTD.|
|0x0BC7|Signtle Inc.|
|0x0BC8|OTF Distribution, LLC|
|0x0BC9|Neuvatek Inc.|
|0x0BCA|Perimeter Technologies, Inc.|
|0x0BCB|Divesoft s.r.o.|
|0x0BCC|Sylvac sa|
|0x0BCD|Amiko srl|
|0x0BCE|Neurosity, Inc.|
|0x0BCF|LL Tec Group LLC|
|0x0BD0|Durag GmbH|
|0x0BD1|Hubei Yuan Times Technology Co., Ltd.|
|0x0BD2|IDEC|
|0x0BD3|Procon Analytics, LLC|
|0x0BD4|ndd Medizintechnik AG|
|0x0BD5|Super B Lithium Power B.V.|
|0x0BD6|Shenzhen Injoinic Technology Co., Ltd.|
|0x0BD8|PURA SCENTS, INC.|
|0x0BDA|Aardex Ltd.|
|0x0BDB|CHAR-BROIL, LLC|
|0x0BDC|TWINKLY SRL|
|0x0BDD|Coroflo Limited|
|0x0BDE|Yale|
|0x0BDF|WINKEY ENTERPRISE (HONG KONG) LIMITED|
|0x0BE0|Koizumi Lighting Technology corp.|
|0x0BE2|OTC engineering|
|0x0BE3|Comtel Systems Ltd.|
|0x0BE4|Deepfield Connect GmbH|
|0x0BE5|ZWILLING J.A. Henckels Aktiengesellschaft|
|0x0BE6|Puratap Pty Ltd|
|0x0BE7|Fresnel Technologies, Inc.|
|0x0BE8|Sensormate AG|
|0x0BE9|Shindengen Electric Manufacturing Co., Ltd.|



Bluetooth SIG Proprietary 

Page 297 of 442 

**Assigned Numbers** / Document 

**2026-06-23** 

|0x0BEA|Twenty Five Seven, prodaja in storitve, d.o.o.|
|---|---|
|0x0BEB|Luna Health, Inc.|
|0x0BED|CORAL-TAIYI Co. Ltd.|
|0x0BEE|LINKSYS USA, INC.|
|0x0BEF|Safetytest GmbH|
|0x0BF0|KIDO SPORTS CO., LTD.|
|0x0BF1|Site IQ LLC|
|0x0BF2|Angel Medical Systems, Inc.|
|0x0BF3|PONE BIOMETRICS AS|
|0x0BF5|T5 tek, Inc.|
|0x0BF6|greenTEG AG|
|0x0BF7|Wacker Neuson SE|
|0x0BF8|Innovacionnye Resheniya|
|0x0BFA|CleanBands Systems Ltd.|
|0x0BFB|Dodam Enersys Co., Ltd|
|0x0BFC|T+A elektroakustik GmbH & Co.KG|
|0x0BFD|Esmé Solutions|
|0x0BFE|Media-Cartec GmbH|
|0x0BFF|Ratio Electric BV|
|0x0C00|MQA Limited|
|0x0C01|NEOWRK SISTEMAS INTELIGENTES S.A.|
|0x0C02|Loomanet, Inc.|
|0x0C03|Puff Corp|
|0x0C04|Happy Health, Inc.|
|0x0C05|Montage Connect, Inc.|
|0x0C06|LED Smart Inc.|
|0x0C07|CONSTRUKTS, INC.|
|0x0C08|limited liability company ”Red”|
|0x0C09|Senic Inc.|
|0x0C0A|Automated Pet Care Products, LLC|
|0x0C0B|aconno GmbH|
|0x0C0C|Mendeltron, Inc.|
|0x0C0D|Mereltron bv|
|0x0C0E|ALEX DENKO CO.,LTD.|
|0x0C0F|AETERLINK|
|0x0C10|Cosmed s.r.l.|



Bluetooth SIG Proprietary 

Page 298 of 442 

**Assigned Numbers** / Document 

**2026-06-23** 

|0x0C11|Gordon Murray Design Limited|
|---|---|
|0x0C12|IoSA|
|0x0C13|Scandinavian Health Limited|
|0x0C14|Fasetto, Inc.|
|0x0C15|Geva Sol B.V.|
|0x0C16|TYKEE PTY. LTD.|
|0x0C17|SomnoMed Limited|
|0x0C18|CORROHM|
|0x0C19|Arlo Technologies, Inc.|
|0x0C1A|Catapult Group International Ltd|
|0x0C1B|Rockchip Electronics Co., Ltd.|
|0x0C1C|GEMU|
|0x0C1D|OFF Line Japan Co., Ltd.|
|0x0C1E|EC sense co., Ltd|
|0x0C1F|LVI Co.|
|0x0C20|COMELIT GROUP S.P.A.|
|0x0C21|Foshan Viomi Electrical Technology Co., Ltd|
|0x0C22|Glamo Inc.|
|0x0C23|KEYTEC,Inc.|
|0x0C24|SMARTD TECHNOLOGIES INC.|
|0x0C25|JURA Elektroapparate AG|
|0x0C26|Performance Electronics, Ltd.|
|0x0C27|Pal Electronics|
|0x0C28|Embecta Corp.|
|0x0C29|DENSO AIRCOOL CORPORATION|
|0x0C2A|Caresix Inc.|
|0x0C2B|GigaDevice Semiconductor Inc.|
|0x0C2C|Zeku Technology (Shanghai) Corp., Ltd.|
|0x0C2D|OTF Product Sourcing, LLC|
|0x0C2E|Easee AS|
|0x0C2F|BEEHERO, INC.|
|0x0C30|McIntosh Group Inc|
|0x0C31|KINDOO LLP|
|0x0C32|Xian Yisuobao Electronic Technology Co., Ltd.|
|0x0C33|Exeger Operations AB|
|0x0C34|BYD Company Limited|



Bluetooth SIG Proprietary 

Page 299 of 442 

**Assigned Numbers** / Document 

**2026-06-23** 

|0x0C35|Thermokon-Sensortechnik GmbH|
|---|---|
|0x0C37|SignalQuest, LLC|
|0x0C38|Noritz Corporation.|
|0x0C39|TIGER CORPORATION|
|0x0C3B|ORB Innovations Ltd|
|0x0C3C|Classified Cycling|
|0x0C3D|Wrmth Corp.|
|0x0C3E|BELLDESIGN Inc.|
|0x0C3F|Stinger Equipment, Inc.|
|0x0C40|HORIBA, Ltd.|
|0x0C41|Control Solutions LLC|
|0x0C42|Heath Consultants Inc.|
|0x0C43|Berlinger & Co. AG|
|0x0C44|ONCELABS LLC|
|0x0C45|Brose Verwaltung SE, Bamberg|
|0x0C47|Epsilon Electronics,lnc|
|0x0C48|VALEO MANAGEMENT SERVICES|
|0x0C49|twopounds gmbh|
|0x0C4A|atSpiro ApS|
|0x0C4B|ADTRAN, Inc.|
|0x0C4C|Orpyx Medical Technologies Inc.|
|0x0C4D|Seekwave Technology Co.,ltd.|
|0x0C4E|Tactile Engineering, Inc.|
|0x0C4F|SharkNinja Operating LLC|
|0x0C50|Imostar Technologies Inc.|
|0x0C51|INNOVA S.R.L.|
|0x0C52|ESCEA LIMITED|
|0x0C53|Taco, Inc.|
|0x0C54|HiViz Lighting, Inc.|
|0x0C55|Zintouch B.V.|
|0x0C56|Rheem Sales Company, Inc.|
|0x0C57|UNEEG medical A/S|
|0x0C58|Hykso Inc.|
|0x0C59|CYBERDYNE Inc.|
|0x0C5A|Lockswitch Sdn Bhd|
|0x0C5B|Alban Giacomo S.P.A.|



Bluetooth SIG Proprietary 

Page 300 of 442 

**Assigned Numbers** / Document 

**2026-06-23** 

|0x0C5C|MGM WIRELESSS HOLDINGS PTY LTD|
|---|---|
|0x0C5D|StepUp Solutions ApS|
|0x0C5E|BlueID GmbH|
|0x0C5F|Wuxi Linkpower Microelectronics Co.,Ltd|
|0x0C60|KEBA Energy Automation GmbH|
|0x0C61|NNOXX, Inc|
|0x0C62|Phiaton Corporation|
|0x0C63|phg Peter Hengstler GmbH + Co. KG|
|0x0C64|dormakaba Holding AG|
|0x0C65|WAKO CO,.LTD|
|0x0C67|TRACKTING S.R.L.|
|0x0C68|Emerja Corporation|
|0x0C69|BLITZ electric motors. LTD|
|0x0C6A|CONSORCIO TRUST CONTROL - NETTEL|
|0x0C6B|GILSON SAS|
|0x0C6C|SNIFF LOGIC LTD|
|0x0C6D|Fidure Corp.|
|0x0C6E|Sensa LLC|
|0x0C6F|Parakey AB|
|0x0C70|SCARAB SOLUTIONS LTD|
|0x0C71|BitGreen Technolabz (OPC) Private Limited|
|0x0C72|StreetCar ORV, LLC|
|0x0C73|Truma Gerätetechnik GmbH & Co. KG|
|0x0C74|yupiteru|
|0x0C75|Embedded Engineering Solutions LLC|
|0x0C77|TEAC Corporation|
|0x0C78|CHARGTRON IOT PRIVATE LIMITED|
|0x0C79|Zhuhai Smartlink Technology Co., Ltd|
|0x0C7A|Triductor Technology (Suzhou), Inc.|
|0x0C7B|PT SADAMAYA GRAHA TEKNOLOGI|
|0x0C7C|Mopeka Products LLC|
|0x0C7D|3ALogics, Inc.|
|0x0C7F|Rochester Sensors, LLC|
|0x0C80|CARDIOID - TECHNOLOGIES, LDA|
|0x0C81|Carrier Corporation|
|0x0C82|NACON|



Bluetooth SIG Proprietary 

Page 301 of 442 

**Assigned Numbers** / Document 

**2026-06-23** 

|0x0C83|Watchdog Systems LLC|
|---|---|
|0x0C84|MAXON INDUSTRIES, INC.|
|0x0C85|Amlogic, Inc.|
|0x0C86|Qingdao Eastsoft Communication Technology Co.,Ltd|
|0x0C87|Weltek Technologies Company Limited|
|0x0C88|Nextivity Inc.|
|0x0C89|AGZZX OPTOELECTRONICS TECHNOLOGY CO., LTD|
|0x0C8A|A.GLOBAL co.,Ltd.|
|0x0C8B|Heavys Inc|
|0x0C8C|T-Mobile USA|
|0x0C8D|tonies GmbH|
|0x0C8E|Technocon Engineering Ltd.|
|0x0C8F|Radar Automobile Sales(Shandong)Co.,Ltd.|
|0x0C90|WESCO AG|
|0x0C91|Yashu Systems|
|0x0C92|Kesseböhmer Ergonomietechnik GmbH|
|0x0C93|Movesense Oy|
|0x0C94|Baxter Healthcare Corporation|
|0x0C95|Gemstone Lights Canada Ltd.|
|0x0C96|H+B Hightech GmbH|
|0x0C97|Deako|
|0x0C99|Vire Health Oy|
|0x0C9A|ALF Inc.|
|0x0C9B|NTT sonority, Inc.|
|0x0C9C|Sunstone-RTLS Ipari Szolgaltato Korlatolt Felelossegu Tarsasag|
|0x0C9D|Ribbiot, INC.|
|0x0C9E|ECCEL CORPORATION SAS|
|0x0C9F|Dragonfly Energy Corp.|
|0x0CA0|BIGBEN|
|0x0CA1|YAMAHA MOTOR CO.,LTD.|
|0x0CA2|XSENSE LTD|
|0x0CA3|MAQUET GmbH|
|0x0CA4|MITSUBISHI ELECTRIC LIGHTING CO, LTD|
|0x0CA5|Princess Cruise Lines, Ltd.|
|0x0CA6|Megger Ltd|
|0x0CA7|Verve InfoTec Pty Ltd|



Bluetooth SIG Proprietary 

Page 302 of 442 

**Assigned Numbers** / Document 

**2026-06-23** 

|0x0CA8|Sonas, Inc.|
|---|---|
|0x0CA9|Mievo Technologies Private Limited|
|0x0CAA|Shenzhen Poseidon Network Technology Co., Ltd|
|0x0CAB|HERUTU ELECTRONICS CORPORATION|
|0x0CAC|Shenzhen Shokz Co.,Ltd.|
|0x0CAD|Shenzhen Openhearing Tech CO., LTD .|
|0x0CAE|Evident Corporation|
|0x0CAF|NEURINNOV|
|0x0CB0|SwipeSense, Inc.|
|0x0CB1|RF Creations|
|0x0CB2|SHINKAWA Sensor Technology, Inc.|
|0x0CB3|janova GmbH|
|0x0CB4|Eberspaecher Climate Control Systems GmbH|
|0x0CB5|Racketry, d. o. o.|
|0x0CB6|THE EELECTRIC MACARON LLC|
|0x0CB7|Cucumber Lighting Controls Limited|
|0x0CB9|seca GmbH & Co. KG|
|0x0CBA|Ameso Tech (OPC) Private Limited|
|0x0CBB|Emlid Tech Kft.|
|0x0CBC|TROX GmbH|
|0x0CBD|Pricer AB|
|0x0CBF|Forward Thinking Systems LLC.|
|0x0CC0|Garnet Instruments Ltd.|
|0x0CC1|CLEIO Inc.|
|0x0CC2|Anker Innovations Limited|
|0x0CC3|HMD Global Oy|
|0x0CC4|ABUS August Bremicker Soehne Kommanditgesellschaft|
|0x0CC5|Open Road Solutions, Inc.|
|0x0CC6|Serial Technology Corporation|
|0x0CC7|SB C&S Corp.|
|0x0CC8|TrikThom|
|0x0CC9|Innocent Technology Co., Ltd.|
|0x0CCA|Cyclops Marine Ltd|
|0x0CCB|NOTHING TECHNOLOGY LIMITED|
|0x0CCC|Kord Defence Pty Ltd|
|0x0CCD|YanFeng Visteon(Chongqing) Automotive Electronic Co.,Ltd|



Bluetooth SIG Proprietary 

Page 303 of 442 

**Assigned Numbers** / Document 

**2026-06-23** 

|0x0CCE|SENOSPACE LLC|
|---|---|
|0x0CCF|Shenzhen CESI Information Technology Co., Ltd.|
|0x0CD0|MooreSilicon Semiconductor Technology (Shanghai) Co., LTD.|
|0x0CD1|Imagine Marketing Limited|
|0x0CD2|EQOM SSC B.V.|
|0x0CD3|TechSwipe|
|0x0CD4|Reoqoo IoT Technology Co., Ltd.|
|0x0CD5|Numa Products, LLC|
|0x0CD6|HHO (Hangzhou) Digital Technology Co., Ltd.|
|0x0CD7|Maztech Industries, LLC|
|0x0CD8|SIA Mesh Group|
|0x0CD9|Minami acoustics Limited|
|0x0CDA|Wolf Steel ltd|
|0x0CDB|Circus World Displays Limited|
|0x0CDC|Ypsomed AG|
|0x0CDD|Alif Semiconductor, Inc.|
|0x0CDF|SHENZHEN CHENYUN ELECTRONICS CO., LTD|
|0x0CE0|VODALOGIC PTY LTD|
|0x0CE1|Regal Beloit America, Inc.|
|0x0CE2|CORVENT MEDICAL, INC.|
|0x0CE3|Taiwan Fuhsing|
|0x0CE4|Off-Highway Powertrain Services Germany GmbH|
|0x0CE5|Amina Distribution AS|
|0x0CE6|McWong International, Inc.|
|0x0CE7|TAG HEUER SA|
|0x0CE8|Dongguan Yougo Electronics Co.,Ltd.|
|0x0CE9|PEAG, LLC dba JLab Audio|
|0x0CEA|HAYWARD INDUSTRIES, INC.|
|0x0CEB|Shenzhen Tingting Technology Co. LTD|
|0x0CEC|Pacific Coast Fishery Services (2003) Inc.|
|0x0CED|CV. NURI TEKNIK|
|0x0CEE|MadgeTech, Inc|
|0x0CEF|POGS B.V.|
|0x0CF0|THOTAKA TEKHNOLOGIES INDIA PRIVATE LIMITED|
|0x0CF1|Midmark|
|0x0CF3|Radio Sound|



Bluetooth SIG Proprietary 

Page 304 of 442 

**Assigned Numbers** / Document 

**2026-06-23** 

|0x0CF4|SOLUX PTY LTD|
|---|---|
|0x0CF5|BOS Balance of Storage Systems AG|
|0x0CF6|OJ Electronics A/S|
|0x0CF7|TVS Motor Company Ltd.|
|0x0CF8|core sensing GmbH|
|0x0CF9|Tamblue Oy|
|0x0CFA|Protect Animals With Satellites LLC|
|0x0CFB|Tyromotion GmbH|
|0x0CFC|ElectronX design|
|0x0CFE|Thule Group AB|
|0x0CFF|Ergodriven Inc|
|0x0D01|KEEPEN|
|0x0D02|Rocky Mountain ATV/MC Jake Wilson|
|0x0D03|MakuSafe Corp|
|0x0D04|Bartec Auto Id Ltd|
|0x0D05|Energy Technology and Control Limited|
|0x0D06|doubleO Co., Ltd.|
|0x0D07|Datalogic S.r.l.|
|0x0D08|Datalogic USA, Inc.|
|0x0D09|Leica Geosystems AG|
|0x0D0A|CATEYE Co., Ltd.|
|0x0D0B|Research Products Corporation|
|0x0D0C|Planmeca Oy|
|0x0D0D|C.Ed. Schulte GmbH Zylinderschlossfabrik|
|0x0D0E|PetVoice Co., Ltd.|
|0x0D0F|Timebirds Australia Pty Ltd|
|0x0D10|JVC KENWOOD Corporation|
|0x0D12|Spartek Systems Inc.|
|0x0D13|MERRY ELECTRONICS CO., LTD.|
|0x0D14|Merry Electronics (S) Pte Ltd|
|0x0D15|Spark|
|0x0D16|Nations Technologies Inc.|
|0x0D17|Akix S.r.l.|
|0x0D18|Bioliberty Ltd|
|0x0D19|C.G. Air Systemes Inc.|
|0x0D1A|Maturix ApS|



Bluetooth SIG Proprietary 

Page 305 of 442 

**Assigned Numbers** / Document 

**2026-06-23** 

|0x0D1B|RACHIO, INC.|
|---|---|
|0x0D1C|LIMBOID LLC|
|0x0D1D|Electronics4All Inc.|
|0x0D1E|FESTINA LOTUS SA|
|0x0D1F|Synkopi, Inc.|
|0x0D20|SCIENTERRA LIMITED|
|0x0D21|Cennox Group Limited|
|0x0D22|Cedarware, Corp.|
|0x0D23|GREE Electric Appliances, Inc. of Zhuhai|
|0x0D24|Japan Display Inc.|
|0x0D25|System Elite Holdings Group Limited|
|0x0D26|Burkert Werke GmbH & Co. KG|
|0x0D27|velocitux|
|0x0D28|FUJITSU COMPONENT LIMITED|
|0x0D29|MIYAKAWA ELECTRIC WORKS LTD.|
|0x0D2A|PhysioLogic Devices, Inc.|
|0x0D2B|Sensoryx AG|
|0x0D2C|SIL System Integration Laboratory GmbH|
|0x0D2D|Cooler Pro, LLC|
|0x0D2E|Advanced Electronic Applications, Inc|
|0x0D2F|Delta Development Team, Inc|
|0x0D30|Laxmi Therapeutic Devices, Inc.|
|0x0D31|SYNCHRON, INC.|
|0x0D32|Badger Meter|
|0x0D33|Micropower Group AB|
|0x0D34|ZILLIOT TECHNOLOGIES PRIVATE LIMITED|
|0x0D35|Universidad Politecnica de Madrid|
|0x0D36|XIHAO INTELLIGENGT TECHNOLOGY CO., LTD|
|0x0D37|Zerene Inc.|
|0x0D38|CycLock|
|0x0D39|Systemic Games, LLC|
|0x0D3A|Frost Solutions, LLC|
|0x0D3B|Lone Star Marine Pty Ltd|
|0x0D3C|SIRONA Dental Systems GmbH|
|0x0D3D|bHaptics Inc.|
|0x0D3E|LUMINOAH, INC.|



Bluetooth SIG Proprietary 

Page 306 of 442 

**Assigned Numbers** / Document 

**2026-06-23** 

|0x0D3F|Vogels Products B.V.|
|---|---|
|0x0D40|SignalFire Telemetry, Inc.|
|0x0D41|CPAC Systems AB|
|0x0D42|TEKTRO TECHNOLOGY CORPORATION|
|0x0D43|Gosuncn Technology Group Co., Ltd.|
|0x0D44|Ex Makhina Inc.|
|0x0D45|Odeon, Inc.|
|0x0D46|Thales Simulation & Training AG|
|0x0D47|Shenzhen DOKE Electronic Co., Ltd|
|0x0D48|Vemcon GmbH|
|0x0D49|Refrigerated Transport Electronics, Inc.|
|0x0D4A|Rockpile Solutions, LLC|
|0x0D4B|Soundwave Hearing, LLC|
|0x0D4D|Optec, LLC|
|0x0D4E|NIKAT SOLUTIONS PRIVATE LIMITED|
|0x0D4F|Movano Inc.|
|0x0D50|NINGBO FOTILE KITCHENWARE CO., LTD.|
|0x0D51|Genetus inc.|
|0x0D52|DIVAN TRADING CO., LTD.|
|0x0D53|Luxottica Group S.p.A|
|0x0D54|ISEKI FRANCE S.A.S|
|0x0D55|NO CLIMB PRODUCTS LTD|
|0x0D56|Wellang.Co,.Ltd|
|0x0D57|Nanjing Xinxiangyuan Microelectronics Co., Ltd.|
|0x0D58|ifm electronic gmbh|
|0x0D59|HYUPSUNG MACHINERY ELECTRIC CO., LTD.|
|0x0D5A|Gunnebo Aktiebolag|
|0x0D5B|Axis Communications AB|
|0x0D5D|Stogger B.V.|
|0x0D5E|Pella Corp|
|0x0D5F|SiChuan Homme Intelligent Technology co.,Ltd.|
|0x0D60|Smart Products Connection, S.A.|
|0x0D61|F.I.P. FORMATURA INIEZIONE POLIMERI - S.P.A.|
|0x0D62|MEBSTER s.r.o.|
|0x0D63|SKF France|
|0x0D64|Southco|



Bluetooth SIG Proprietary 

Page 307 of 442 

**Assigned Numbers** / Document 

**2026-06-23** 

|0x0D65|Molnlycke Health Care AB|
|---|---|
|0x0D66|Hendrickson USA , L.L.C|
|0x0D67|BLACK BOX NETWORK SERVICES INDIA PRIVATE LIMITED|
|0x0D68|Status Audio LLC|
|0x0D69|AIR AROMA INTERNATIONAL PTY LTD|
|0x0D6A|Helge Kaiser GmbH|
|0x0D6B|Crane Payment Innovations, Inc.|
|0x0D6C|Ambient IoT Pty Ltd|
|0x0D6D|DYNAMOX S/A|
|0x0D6E|Look Cycle International|
|0x0D6F|Closed Joint Stock Company NVP BOLID|
|0x0D70|Kindhome|
|0x0D71|Kiteras Inc.|
|0x0D72|Earfun Technology (HK) Limited|
|0x0D73|iota Biosciences, Inc.|
|0x0D74|ANUME s.r.o.|
|0x0D75|Indistinguishable From Magic, Inc.|
|0x0D76|i-focus Co.,Ltd|
|0x0D77|DualNetworks SA|
|0x0D78|MITACHI CO.,LTD.|
|0x0D79|VIVIWARE JAPAN, Inc.|
|0x0D7A|Xiamen Intretech Inc.|
|0x0D7B|MindMaze SA|
|0x0D7C|BeiJing SmartChip Microelectronics Technology Co.,Ltd|
|0x0D7D|Taiko Audio B.V.|
|0x0D7E|Daihatsu Motor Co., Ltd.|
|0x0D7F|Konova|
|0x0D80|Gravaa B.V.|
|0x0D81|Beyerdynamic GmbH & Co. KG|
|0x0D82|VELCO|
|0x0D83|ATLANTIC SOCIETE FRANCAISE DE DEVELOPPEMENT THERMIQUE|
|0x0D84|Testo SE & Co. KGaA|
|0x0D85|SEW-EURODRIVE GmbH & Co KG|
|0x0D86|ROCKWELL AUTOMATION, INC.|
|0x0D87|Quectel Wireless Solutions Co., Ltd.|
|0x0D89<br>Bluetooth SI|Nanohex Corp<br>G Proprietary<br>Pag|



Page 308 of 442 

**Assigned Numbers** / Document 

**2026-06-23** 

|0x0D8A|Simply Embedded Inc.|
|---|---|
|0x0D8B|Software Development, LLC|
|0x0D8C|Ultimea Technology (Shenzhen) Limited|
|0x0D8D|RF Electronics Limited|
|0x0D8E|Optivolt Labs, Inc.|
|0x0D8F|Canon Electronics Inc.|
|0x0D90|LAAS ApS|
|0x0D91|Beamex Oy Ab|
|0x0D92|TACHIKAWA CORPORATION|
|0x0D93|HagerEnergy GmbH|
|0x0D95|Hunter Industries Incorporated|
|0x0D96|NEOKOHM SISTEMAS ELETRONICOS LTDA|
|0x0D97|Zhejiang Huanfu Technology Co., LTD|
|0x0D98|E.F. Johnson Company|
|0x0D99|Caire Inc.|
|0x0D9A|Yeasound (Xiamen) Hearing Technology Co., Ltd|
|0x0D9B|Boxyz, Inc.|
|0x0D9C|Skytech Creations Limited|
|0x0D9D|Cear, Inc.|
|0x0D9E|Impulse Wellness LLC|
|0x0D9F|MML US, Inc|
|0x0DA0|SICK AG|
|0x0DA1|Fen Systems Ltd.|
|0x0DA2|KIWI.KI GmbH|
|0x0DA3|Airgraft Inc.|
|0x0DA4|HP Tuners|
|0x0DA5|PIXELA CORPORATION|
|0x0DA6|Generac Corporation|
|0x0DA7|Novoferm tormatic GmbH|
|0x0DA8|Airwallet ApS|
|0x0DA9|Inventronics GmbH|
|0x0DAA|Shenzhen EBELONG Technology Co., Ltd.|
|0x0DAB|Efento|
|0x0DAC|ITALTRACTOR ITM S.P.A.|
|0x0DAE|TITUM AUDIO, INC.|
|0x0DAF|Hexagon Aura Reality AG|



Bluetooth SIG Proprietary 

Page 309 of 442 

**Assigned Numbers** / Document 

**2026-06-23** 

|0x0DB0|Invisalert Solutions, Inc.|
|---|---|
|0x0DB1|TELE System Communications Pte. Ltd.|
|0x0DB2|Whirlpool|
|0x0DB3|SHENZHEN REFLYING ELECTRONIC CO., LTD|
|0x0DB4|Franklin Control Systems|
|0x0DB5|Djup AB|
|0x0DB6|SAFEGUARD EQUIPMENT, INC.|
|0x0DB7|Morningstar Corporation|
|0x0DB8|Shenzhen Chuangyuan Digital Technology Co., Ltd|
|0x0DB9|CompanyDeep Ltd|
|0x0DBA|Veo Technologies ApS|
|0x0DBB|Nexis Link Technology Co., Ltd.|
|0x0DBC|Felion Technologies Company Limited|
|0x0DBD|MAATEL|
|0x0DBE|HELLA GmbH & Co. KGaA|
|0x0DBF|HWM-Water Limited|
|0x0DC0|Shenzhen Jahport Electronic Technology Co., Ltd.|
|0x0DC1|NACHI-FUJIKOSHI CORP.|
|0x0DC2|Cirrus Research plc|
|0x0DC3|GEARBAC TECHNOLOGIES INC.|
|0x0DC4|Hangzhou NationalChip Science & Technology Co.,Ltd|
|0x0DC5|DHL|
|0x0DC6|Levita|
|0x0DC7|MORNINGSTAR FX PTE. LTD.|
|0x0DC8|ETO GRUPPE TECHNOLOGIES GmbH|
|0x0DC9|farmunited GmbH|
|0x0DCA|Aptener Mechatronics Private Limited|
|0x0DCB|GEOPH, LLC|
|0x0DCC|Trotec GmbH|
|0x0DCD|Astra LED AG|
|0x0DCE|NOVAFON - Electromedical devices limited liability company|
|0x0DCF|KUBU SMART LIMITED|
|0x0DD0|ESNAH|
|0x0DD1|OrangeMicro Limited|
|0x0DD2|Fresh n Rebel B.V.|
|0x0DD3|Global Satellite Engineering|



Bluetooth SIG Proprietary 

Page 310 of 442 

**Assigned Numbers** / Document 

**2026-06-23** 

|0x0DD4|KOQOON GmbH & Co.KG|
|---|---|
|0x0DD5|BEEPINGS|
|0x0DD6|MODULAR MEDICAL, INC.|
|0x0DD7|Xiant Technologies, Inc.|
|0x0DD9|SCHELL GmbH & Co. KG|
|0x0DDA|Minebea Intec GmbH|
|0x0DDB|KAGA FEI Co., Ltd.|
|0x0DDC|AUTHOR-ALARM, razvoj in prodaja avtomobilskih sistemov proti kraji, d.o.o.|
|0x0DDD|Tozoa LLC|
|0x0DDE|SHENZHEN DNS INDUSTRIES CO., LTD.|
|0x0DDF|Shenzhen Lunci Technology Co., Ltd|
|0x0DE0|KNOG PTY. LTD.|
|0x0DE1|Outshiny India Private Limited|
|0x0DE2|TAMADIC Co., Ltd.|
|0x0DE3|Shenzhen MODSEMI Co., Ltd|
|0x0DE4|EMBEINT INC|
|0x0DE5|Ehong Technology Co.,Ltd|
|0x0DE6|DEXATEK Technology LTD|
|0x0DE7|Dendro Technologies, Inc.|
|0x0DE8|Vivint, Inc.|
|0x0DE9|General Laser GmbH|
|0x0DEA|Kathrein Solutions GmbH|
|0x0DEB|Fitz Inc.|
|0x0DEC|ATEGENOS PHARMACEUTICALS INC|
|0x0DED|Flextronic GmbH|
|0x0DEE|Safety Swim LLC|
|0x0DEF|SING SUN TECHNOLOGY (INTERNATIONAL) LIMITED|
|0x0DF0|Woncan (Hong Kong) Limited|
|0x0DF1|iFLYTEK (Suzhou) Technology Co., Ltd.|
|0x0DF2|Weber-Stephen Products LLC|
|0x0DF3|hDrop Technologies Inc.|
|0x0DF4|REEKON TOOLS INC.|
|0x0DF5|Delta Faucet Company|
|0x0DF6|Mutrack Co., Ltd|
|0x0DF7|Hangzhou Zhaotong Microelectronics Co., Ltd.|
|0x0DF8<br>Bluetooth SI|Chengdu CSCT Microelectronics Co., Ltd.<br>G Proprietary<br>Page 3|



Page 311 of 442 

**Assigned Numbers** / Document 

**2026-06-23** 

|0x0DF9|Belusun Technology Ltd.|
|---|---|
|0x0DFA|Shenzhen Matches IoT Technology Co., Ltd.|
|0x0DFB|Beidou Intelligent Connected Vehicle Technology Co., Ltd.|
|0x0DFC|SOJI ELECTRONICS JOINT STOCK COMPANY|
|0x0DFD|BH Technologies|
|0x0DFE|Haptech, Inc.|
|0x0DFF|WaveRF, Corp.|
|0x0E00|SHENZHEN SOUNDSOUL INFORMATION TECHNOLOGY CO.,LTD|
|0x0E01|Wuhu Mengbo Technology Co., Ltd.|
|0x0E02|PROSYS DEV LIMITED|
|0x0E03|Shenzhen eMeet technology Co.,Ltd|
|0x0E04|Doro AB|
|0x0E05|SUREPULSE MEDICAL LIMITED|
|0x0E06|iodyne, LLC|
|0x0E07|Pinpoint GmbH|
|0x0E08|Heinrich Kopp GmbH|
|0x0E09|Evolutive Systems SL|
|0x0E0B|Sounding Audio Industrial Ltd.|
|0x0E0C|Yuanfeng Technology Co., Ltd.|
|0x0E0D|FrontAct Co., Ltd.|
|0x0E0F|SenseWorks Tecnologia Ltda.|
|0x0E10|Eko Health, Inc.|
|0x0E11|Wanzl GmbH & Co. KGaA|
|0x0E12|CLEVER LOGGER TECHNOLOGIES PTY LIMITED|
|0x0E13|ASYSTOM|
|0x0E14|Heilongjiang Tianyouwei Electronics Co.,Ltd.|
|0x0E15|Eastern Partner Limited|
|0x0E16|Xiamen RUI YI Da Electronic Technology Co.,Ltd|
|0x0E17|Ad Hoc Electronics, llc.|
|0x0E18|Hangzhou Microimage Software Co.,Ltd.|
|0x0E19|Hive-Zox International SA|
|0x0E1A|Sensovo GmbH|
|0x0E1B|Time Location Systems AS|
|0x0E1C|SHENZHEN DIGITECH CO., LTD|
|0x0E1D|Capte B.V.|
|0x0E1E|9512-5837 QUEBEC INC.|



Bluetooth SIG Proprietary 

Page 312 of 442 

**Assigned Numbers** / Document 

**2026-06-23** 

|0x0E1F|Blecon Ltd|
|---|---|
|0x0E20|CFLAB TEKNOLOJI TICARET LIMITED SIRKETI|
|0x0E21|FOGO|
|0x0E22|HITO INC|
|0x0E23|MS kajak7 UG (limited liability)|
|0x0E24|Avedis Zildjian Co.|
|0x0E25|Hangzhou Hikvision Digital Technology Co., Ltd.|
|0x0E26|LIHJOEN SPEED METER CO., LTD.|
|0x0E27|NextSense, Inc.|
|0x0E28|PatchRx, Inc.|
|0x0E29|Flipper Devices Inc.|
|0x0E2A|Huizhou Foryou General Electronics Co., Ltd.|
|0x0E2B|JE electronic a/s|
|0x0E2C|9313-7263 Quebec inc.|
|0x0E2D|ECARX (Hubei) Tech Co.,Ltd.|
|0x0E2E|NIHON KOHDEN CORPORATION|
|0x0E2F|ONWI|
|0x0E30|Primax Electronics Ltd.|
|0x0E31|AlphaTheta Corporation|
|0x0E32|PACIFIC INDUSTRIAL CO., LTD.|
|0x0E33|Crescent NV|
|0x0E34|Vermis, software solutions llc|
|0x0E35|SNAPPWISH LLC|
|0x0E36|Cousins and Sears LLC|
|0x0E37|CESYS Gesellschaft für angewandte Mikroelektronik mbH|
|0x0E38|SLOC GmbH|
|0x0E39|IRES Infrarot Energie Systeme GmbH|
|0x0E3A|OFIVE LIMITED|
|0x0E3B|Swift IOT Tech (Shenzhen) Co., LTD.|
|0x0E3C|Viselabs|
|0x0E3D|Walmart Inc.|
|0x0E3E|VANBOX|
|0x0E3F|Wiser Devices, LLC|
|0x0E40|WKD Labs Ltd|
|0x0E41|Asustek Computer Inc.|
|0x0E42|Z-ONE Technology Co., Ltd.|



Bluetooth SIG Proprietary 

Page 313 of 442 

**Assigned Numbers** / Document 

**2026-06-23** 

|0x0E43|InnoVision Medical Technologies, LLC|
|---|---|
|0x0E44|QUANTATEC|
|0x0E45|Filo Srl|
|0x0E46|SOUNDUCT|
|0x0E47|Hosiden Besson Limited|
|0x0E48|KARLUNA MUHENDISLIK SANAYI VE TICARET ANONIM SIRKETI|
|0x0E49|Deone (Shanghai) Communication & Technology Co., Ltd|
|0x0E4A|Nitto Denko Corporation|
|0x0E4B|PUDSEY DIAMOND ENGINEERING LIMITED|
|0x0E4C|Luxshare Precision Industry Co., Ltd.|
|0x0E4D|Brooksee, Inc.|
|0x0E4E|QSC, LLC|
|0x0E4F|eBet Gaming Sytems Pty Limited|
|0x0E50|Zhejiang Desman Intelligent Technology Co., Ltd.|
|0x0E51|Dyaco International Inc.|
|0x0E52|Aquana, LLC|
|0x0E53|MINIRIG|
|0x0E54|After Technologies AS|
|0x0E55|New Cosmos Electric Co., Ltd.|
|0x0E56|INEPRO Metering B.V.|
|0x0E57|Altina Inc.|
|0x0E58|Urban Armor Gear, LLC|
|0x0E59|Loewe Technology GmbH|
|0x0E5A|OmniWave Microelectronics Shanghai Co., Ltd|
|0x0E5B|Wuhu Hongjing Electronic Co.,Ltd|
|0x0E5C|Rocoto Ltd|
|0x0E5D|L.T.H. Electronics Limited|
|0x0E5E|SHAPER TOOLS, INC.|
|0x0E5F|Ruptela|
|0x0E60|Ant Group Co., Ltd.|
|0x0E61|Queclink Wireless Solutions Co., Ltd.|
|0x0E62|GOKI PTY LTD|
|0x0E63|LAST LOCK INC.|
|0x0E64|HuiTong intelligence Company Limited|
|0x0E65|Daikin Industries, LTD|
|0x0E66|Shenzhen Baseus Technology Co., Ltd.|



Bluetooth SIG Proprietary 

Page 314 of 442 

**Assigned Numbers** / Document 

**2026-06-23** 

|0x0E67|NEXT DEVICES LTDA|
|---|---|
|0x0E69|Megatronix (Beijing) Technology Co., Ltd|
|0x0E6A|Hyena Inc.|
|0x0E6B|Shenzhen Goodocom Information Technology Co., Ltd.|
|0x0E6C|RIGH, INC.|
|0x0E6F|OpConnect, Inc.|
|0x0E70|Powerstick.com|
|0x0E71|ENABLEWEAR LLC|
|0x0E72|TOR.AI LIMITED|
|0x0E73|Adventures of the Persistently Impaired (and other tales) Limited|
|0x0E74|Rocky Radios LLC|
|0x0E75|Le Touch (Shenzhen) Electronics Co., Ltd.|
|0x0E76|Guangdong Nanguang Photo&Video Systems Co., Ltd.|
|0x0E77|MOBILE TECH, INC.|
|0x0E78|HONG KONG COMMUNICATIONS COMPANY LIMITED|
|0x0E79|Neptune First OU|
|0x0E7A|Vivago Oy|
|0x0E7B|Circular|
|0x0E7C|final Inc.|
|0x0E7D|Jiangsu XinTongda Electric Technology Co.,Ltd.|
|0x0E7E|Archon Controls LLC|
|0x0E7F|SZR-Dev UG|
|0x0E80|IQNEXXT Solutions GmbH|
|0x0E81|Guangdong Hengqin Xingtong Technology Co.,ltd.|
|0x0E82|CHEVALIER TECH LIMITED|
|0x0E83|SPRiNTUS GmbH|
|0x0E84|Tymphany HK Ltd|
|0x0E85|TigerLight, Inc.|
|0x0E86|Mercury Marine, a division of Brunswick Corporation|
|0x0E87|OpenTech Alliance, Inc.|
|0x0E88|Skewered Fencing, LLC|
|0x0E89|Brudden|
|0x0E8A|Tele-Radio i Lysekil AB|
|0x0E8B|Allgon AB|
|0x0E8C|Gopod Group Holding Limited|
|0x0E8D|Celebrities Management Private Limited|



Bluetooth SIG Proprietary 

Page 315 of 442 

**Assigned Numbers** / Document 

**2026-06-23** 

|0x0E8E|Nobest Inc|
|---|---|
|0x0E8F|Avetos Design LLC|
|0x0E90|RainMaker Solutions, Inc.|
|0x0E91|Identita Inc.|
|0x0E92|Glutz AG|
|0x0E93|MIV ELECTRONICS, LTD|
|0x0E94|Tactrix|
|0x0E95|Viaanix, Inc.|
|0x0E96|Skeed,co,Ltd.|
|0x0E97|kokoromil Inc.|
|0x0E98|Chromatic Inc.|
|0x0E99|Medibound, Inc.|
|0x0E9A|Scanbro OU|
|0x0E9B|Panasonic Automotive Systems Co., Ltd.|
|0x0E9C|Andrews & Arnold Ltd|
|0x0E9D|Audinor ApS|
|0x0E9E|Travelxp India Private Limited|
|0x0E9F|Owlet Baby Care Inc.|
|0x0EA0|ENLESS WIRELESS|
|0x0EA1|Culligan International Company|
|0x0EA2|QIKCONNEX LLC|
|0x0EA3|OLIS ELECTRONICS, LLC|
|0x0EA4|BiTECH Automotive (Wuhu) Co.,Ltd|
|0x0EA6|AMG Lab LLC|
|0x0EA7|BrickXter GmbH|
|0x0EA8|Dongguan Trangjan Industrial Co., Ltd|
|0x0EA9|Makichie Co., Ltd.|
|0x0EAA|Hive Soundz inc.|
|0x0EAB|STEYR Sport GmbH|
|0x0EAC|Dynetrex Solutions Inc.|
|0x0EAD|OPTRON Co., Ltd.|
|0x0EAE|BHClears Microelectronics (Shanghai) Co., Ltd.|
|0x0EAF|Unfolded Circle ApS|
|0x0EB0|FactorySense|
|0x0EB1|WearNex Limited|
|0x0EB2|PRADCO Outdoor Brands|



Bluetooth SIG Proprietary 

Page 316 of 442 

**Assigned Numbers** / Document 

**2026-06-23** 

|0x0EB3|Zucchetti Axess|
|---|---|
|0x0EB4|BLUEFIN DATA, LLC|
|0x0EB5|Preseed Japan Corporation|
|0x0EB6|Server Products, Inc.|
|0x0EB7|Embedded Solutions LLC|
|0x0EB9|egojin co,.ltd|
|0x0EBA|THERMY LTD|
|0x0EBB|Asahi Denso Co.,Ltd.|
|0x0EBC|GP Acoustics International Limited|
|0x0EBD|Tongfang Health Technology (Beijing) Co., Ltd.|
|0x0EBE|Deity Acoustic Technology Co.|
|0x0EBF|Schulte-Schlagbaum AG|
|0x0EC0|WEST inx Ltd.|
|0x0EC1|Crossdoor|
|0x0EC2|High Entropy, LLC|
|0x0EC3|Herschel Infrared Ltd|
|0x0EC4|PACIFIC MARINE BATTERIES PTY. LIMITED|
|0x0EC5|Alibaba (China) Co., Ltd.|
|0x0EC6|AuthGate B.V.|
|0x0EC7|Canyon Bicycles GmbH|
|0x0EC8|Codie LLC|
|0x0EC9|NeuroPace Inc|
|0x0ECA|NexRev LLC|
|0x0ECB|Zhong Shan City Richsound Electronic Industrial Ltd.|
|0x0ECC|Shenzhen NEOECO Technology Co., Ltd.|
|0x0ECD|Nature Inc.|
|0x0ECE|Guangzhou Honor Microelectronic Co.,Ltd.|
|0x0ECF|GGEC America, Inc.|
|0x0ED0|Gibson, Inc.|
|0x0ED1|VINYL MATT MEDIA LIMITED|
|0x0ED2|SHENZHEN BESTWAY ELECTRONICS CO.,LTD|
|0x0ED3|Lichens Innovation inc.|
|0x0ED4|Goerdyna Group Co., Ltd|
|0x0ED5|Relish Technologies Limited|
|0x0ED6|Quintessential Design, Inc.|
|0x0ED7|CS INSTRUMENTS GmbH & Co.KG|



Bluetooth SIG Proprietary 

Page 317 of 442 

**Assigned Numbers** / Document 

**2026-06-23** 

|0x0ED8|DORAN MFG. LLC|
|---|---|
|0x0ED9|Overhead Door Corporation|
|0x0EDA|Kodira GmbH|
|0x0EDB|ShenZhen BoYiChuangXin|
|0x0EDC|Shenzhen Zoqin Technology Co., Ltd.|
|0x0EDD|Ceridwen Limited|
|0x0EDE|Sony Honda Mobility Inc.|
|0x0EDF|Dynaudio A/S|
|0x0EE0|MAERSK CONTAINER INDUSTRY A/S|
|0x0EE1|MA MICRO LIMITED|
|0x0EE2|shenzhen hongever technology Co,. Ltd|
|0x0EE3|TAMRON Co., Ltd.|
|0x0EE4|CAPTEMP, LDA|
|0x0EE5|Ambient Life Inc.|
|0x0EE6|NOCTRIX HEALTH, INC|
|0x0EE7|RICKARD AIR DIFFUSION (PTY) LTD|
|0x0EE8|SG Armaturen AS|
|0x0EE9|PIXEL TI IND. E COM PROD ELETRONICOS|
|0x0EEA|Core Devices LLC|
|0x0EEB|Fujita Electric Works, Ltd|
|0x0EEC|Willow Laboratories, Inc.|
|0x0EED|BBC Bircher AG|
|0x0EEE|ELLEA INGEGNERIA SRL UNIPERSONALE|
|0x0EEF|Q42 Internet B.V.|
|0x0EF0|Seaward Electronic|
|0x0EF1|Wuxi Does IOT Co., Ltd|
|0x0EF2|CAPTAIN BLINK|
|0x0EF3|Monil AS|
|0x0EF5|LS ELECTRIC Co., Ltd.|
|0x0EF6|Shenzhen Cyber Innovation Technology Co., Ltd.|
|0x0EF7|Trackonomy Systems, Inc.|
|0x0EF8|IoT Solutions Malta Limited|
|0x0EF9|Aseptico, Inc.|
|0x0EFA|Sensear Pty Ltd|
|0x0EFB|BLUEPROVIDERZ LLC|
|0x0EFC|Jano Life Inc.|



Bluetooth SIG Proprietary 

Page 318 of 442 

**Assigned Numbers** / Document 

**2026-06-23** 

|0x0EFD|PRIMES GmbH|
|---|---|
|0x0EFE|Teledyne Instruments, Inc.|
|0x0EFF|Healthcare Technology Limited|
|0x0F00|TRACERCO LIMITED|
|0x0F01|HAPPLABS SOFTWARE PRIVATE LIMITED|
|0x0F02|GE HEALTHCARE TECHNOLOGIES INC.|
|0x0F03|Olibra LLC|
|0x0F04|Lumen Labs (HK) Ltd|
|0x0F05|SHENZHEN GWSTAI TECHNOLOGY CO.,LTD|
|0x0F06|SHENZHEN POWEROAK NEWENER CO., LTD|
|0x0F07|Tech OVN Private Limited|
|0x0F08|Lezyne USA Inc.|
|0x0F09|ANC CHINA LIMITED|
|0x0F0A|LION GROUP, INC.|
|0x0F0B|Brightway Innovation Intelligent Technology (Suzhou) Co., Ltd.|
|0x0F0C|Hitachi Industrial Equipment Systems Co.,Ltd.|
|0x0F0D|Maennl Elektronik GmbH|
|0x0F0E|IDEATRONIK Limited Liability Company|
|0x0F0F|caive Inc.|
|0x0F10|ShenZhen Doctors of Intelligence & Technology Co.,Ltd|
|0x0F11|Deep and Steep LLC|
|0x0F12|Endur ID, Inc.|
|0x0F13|Freshape SA|
|0x0F14|PreEvnt, LLC|
|0x0F15|Oval Corporation|
|0x0F16|ADHD Friendly co.Ltd|
|0x0F17|RORENTECH Co., Ltd.|
|0x0F18|iKeyless, LLC|
|0x0F19|Shenzhen SuperSound Technology Co.,Ltd|
|0x0F1A|FLO SCIENCES, LLC|
|0x0F1B|PalatiumCare LLC|
|0x0F1C|Edge Semiconductors Inc.|
|0x0F1D|ZIMMERMANN PV-Steel Group GmbH & Co. KG|
|0x0F1E|Yolni Inc.|
|0x0F1F|Ninebot (Changzhou) Tech Co., Ltd.|
|0x0F20|Beijing Spring Creation Technology Co., Ltd.|



Bluetooth SIG Proprietary 

Page 319 of 442 

**Assigned Numbers** / Document 

**2026-06-23** 

|0x0F21|Rokk Limited|
|---|---|
|0x0F22|ONESPACE TECHNOLOGIES (PTY) LTD|
|0x0F23|Yuquan Semiconductor (Xiamen) Co., Ltd.|
|0x0F24|Easy Measure Co., Ltd.|
|0x0F25|Sichuan Changhong Neonet Technologies Co.,Ltd.|
|0x0F26|Centromere Holding B.V.|
|0x0F27|Global Link Distribution Corp.|
|0x0F28|Amp Fit Israel LTD|
|0x0F29|Trezor Company s.r.o.|
|0x0F2A|KELLER Druckmesstechnik AG|
|0x0F2B|ElitEngineering LLC|
|0x0F2C|Vision Group Inc.|
|0x0F2D|ClipsClips LLC|
|0x0F2E|BRITZ INTERNATIONAL CO.,LTD|
|0x0F2F|ThingCo Limited|
|0x0F30|Opal Camera, Inc.|
|0x0F31|TECHNOLOGIES FOR FREERIDE s.r.o|
|0x0F32|FLINTEC UK LIMITED|
|0x0F33|TETNET|
|0x0F34|NINGBO SHARKWARD ELECTRONICS CO.,LTD|
|0x0F35|Audeze LLC|
|0x0F36|NODER Joint Stock Company|
|0x0F37|Schueco International KG|
|0x0F38|GILL INSTRUMENTS LIMITED|
|0x0F39|Seitron Spa|
|0x0F3A|Southern Audio Services, Inc|
|0x0F3B|SANWA NEWTEC CO.,LTD.|
|0x0F3C|Mobile Technology Solutions LLC|
|0x0F3D|Vitio Medical S.L.|
|0x0F3E|Salyx Medical Inc.|
|0x0F3F|Landig + Lava GmbH & Co. KG|
|0x0F40|Health Data Insight C.I.C.|
|0x0F41|BONX INC.|
|0x0F42|LINKEDCHIP TECHNOLOGY INC|
|0x0F43|12mm Health Technology (Hainan) Co., Ltd.|
|0x0F44|MAVERICK ENERGY SOLUTIONS INTERNATIONAL,INC|



Bluetooth SIG Proprietary 

Page 320 of 442 

**Assigned Numbers** / Document 

**2026-06-23** 

|0x0F45|Tactica Defense LLC|
|---|---|
|0x0F46|Huizhou Meicanxin Electronics Technology Co.,Ltd|
|0x0F47|Lodestar Technology Inc.|
|0x0F48|Shenzhen Xinfeiyi Technology Co., Ltd.|
|0x0F49|IYO INC.|
|0x0F4A|Mitsubishi Motors Corporation|
|0x0F4B|desamisCo.,Ltd.|
|0x0F4C|JL WORLD CORPORATION LIMITED|
|0x0F4D|Qulinda AB|
|0x0F4E|Ningbo Dooya Mechanic & Electronic Technology Co., Ltd|
|0x0F4F|G-Vision GmbH|
|0x0F50|Lantern Innovations Incorporated|
|0x0F51|ebm-papst Mulfingen GmbH & Co. KGaA & Co. KG|
|0x0F52|Shenzhen Yunke Intelligent Co.Ltd|
|0x0F53|Fledt & Meiton Marin AB|
|0x0F54|ContiTech Deutschland GmbH|
|0x0F55|Sensoteq Ltd|
|0x0F56|Astute Access Group Limited|
|0x0F57|Silverlake Technologies|
|0x0F58|Shenzhen Guo-link Technology Co.,Ltd.|
|0x0F59|Silicon Vandals PTY LTD|
|0x0F5A|Vibe Energy B.V.|
|0x0F5B|shanghai fudan electronics group company Co. Ltd|
|0x0F5C|Micro Technology Services, Inc.|
|0x0F5D|Leapcraft ApS|
|0x0F5E|WUXI WEIDA INTELLIGENT ELECTRONICS CO.,LTD.|
|0x0F5F|ROBSON SRL|
|0x0F60|Quilt Systems, Inc.|
|0x0F61|HyolimXE Co., Ltd.|
|0x0F62|Kehwin Technologies Co. Ltd.|
|0x0F63|WATTER, Inc.|
|0x0F64|SafeNow GmbH|
|0x0F65|CZV, Inc|
|0x0F66|BIGREDBEE, LLC|
|0x0F67|White Eagle Sonic Technologies, Inc.|
|0x0F68|Navico|



Bluetooth SIG Proprietary 

Page 321 of 442 

**Assigned Numbers** / Document 

**2026-06-23** 

|0x0F69|Ancoe Industry Corporation|
|---|---|
|0x0F6A|Roam Devices LLC|
|0x0F6B|Skywalk Inc.|
|0x0F6C|Biogents AG|
|0x0F6D|MODRETRO, INC.|
|0x0F6E|iSi Wearable Safety GmbH|
|0x0F6F|onanoff limited|
|0x0F70|TECHFLITTER SOLUTIONS PRIVATE LIMITED|
|0x0F71|PIRITIZ LLC|
|0x0F72|FLORLINK, INC.|
|0x0F73|Keycafe Inc.|
|0x0F74|Sperry Labs LLC|
|0x0F75|Senseonics, Incorporated|
|0x0F76|simatec ag|
|0x0F77|CROSS BRAIN CO.,Ltd.|
|0x0F78|Also, Inc.|
|0x0F79|Walter Mueller Inc. for Industrial Electronics|
|0x0F7A|SHENZHEN GUANG QI GUO CHUANG TECHNOLOGY CO.,LTD|
|0x0F7B|SKYROAM, INC.|
|0x0F7C|Polk Audio|
|0x0F7D|eQ-3 AG|
|0x0F7E|shenzhen Holyiot Technology Co.,Ltd|
|0x0F7F|Badass eBikes GmbH|
|0x0F80|Hydro Electronic Devices, Inc.|
|0x0F81|IN Phase International lTD|
|0x0F82|Amon Co.Ltd.|
|0x0F83|Calpeda S.p.A.|
|0x0F84|KT Micro, Inc.|
|0x0F85|Hangzhou Sciener Smart Technology Co., Ltd.|
|0x0F86|Lakecrest Pty Ltd|
|0x0F87|Anacove Inc.|
|0x0F88|Kohler Ventures, Inc.|
|0x0F89|STOREIO TECHNOLOGIES LTD|
|0x0F8A|Innogando S.L.|
|0x0F8B|A.Y. McDonald Mfg. Co.|
|0x0F8C|ROYAL PARTS CO.,LTD|



Bluetooth SIG Proprietary 

Page 322 of 442 

**Assigned Numbers** / Document 

**2026-06-23** 

|0x0F8D|JUREN CO., LTD.|
|---|---|
|0x0F8E|Biceek Inc|
|0x0F8F|SIGNUM INTELLIGENCE LTD|
|0x0F90|MOBI7 TECNOLOGIA EM MOBILIDADE S.A.|
|0x0F91|Valostra Digital Ventures LLP|
|0x0F92|Ethos Group, Inc.|
|0x0F93|Aktiebolaget Ebeco|
|0x0F94|IDX Company, Ltd.|
|0x0F95|Dongguang Huasoo Automation Technology Company Limited|
|0x0F96|SEIKOIST, INC|
|0x0F97|SPX Aids to Navigation Oy|
|0x0F98|Fitnexa Inc|
|0x0F99|DEZINE GROUP LLC|
|0x0F9A|Eiritsu Electronics Industry Co., Ltd.|
|0x0F9B|Hanshow Technology Co.,Ltd.|
|0x0F9C|WePower Technologies LLC|
|0x0F9D|IRTrans GmbH|
|0x0F9E|Hibino Corporation|
|0x0F9F|Seiko Future Creation Inc.|
|0x0FA0|DP IOT|
|0x0FA1|PALRED RETAIL PRIVATE LIMITED|
|0x0FA2|BIA NEUROSCIENCE INC.|
|0x0FA3|B.E.G. Brueck Electronic GmbH|
|0x0FA4|Nice Spa|
|0x0FA5|Avanos Medical, Inc.|
|0x0FA6|Ugreen Group Limited|
|0x0FA7|spheretek japan|
|0x0FA8|Senti Technologies Private Limited|
|0x0FA9|Ranch Systems, Inc.|
|0x0FAA|Suzhou Fanxi Technology Co., Ltd.|
|0x0FAB|Geo Radar AI Private Limited|
|0x0FAC|Flitsmeister B.V.|
|0x0FAD|The AI Toy Company|
|0x0FAE|Capacite|
|0x0FAF|Terasite Technologies|
|0x0FB0|General Resistance, LLC|



Bluetooth SIG Proprietary 

Page 323 of 442 

**Assigned Numbers** / Document 

**2026-06-23** 

|0x0FB1|Avivomed Inc.|
|---|---|
|0x0FB2|PLAUD Inc.|
|0x0FB3|Hooked Society Oy Ltd|
|0x0FB4|StoneDevices|
|0x0FB5|Hangzhou Nano IC Technologies Co,. Ltd|
|0x0FB6|TQ-Systems GmbH|
|0x0FB7|Hapn Holdings, LLC|
|0x0FB8|SITERWELL ELECTRONICS CO.,LIMITED|
|0x0FB9|Shenzhen Mutual Technology Co., Ltd|
|0x0FBA|Cosonic Intelligent Technologies Co., Ltd.|
|0x0FBB|Rollease Acmeda, Inc.|
|0x0FBC|ATANS TECHNOLOGY INC.|
|0x0FBD|Locus Robotics Corp.|
|0x0FBE|TaigaIoT|
|0x0FBF|Tridentify AB|
|0x0FC0|Littlebird Connected Care, Inc.|
|0x0FC1|Air Automotive tracking Inc.|
|0x0FC2|ISSPRO, INC|
|0x0FC3|Shenzhen Jimi loT Co.,Ltd|
|0x0FC4|FulScience Automotive Electronics Co., Ltd.|
|0x0FC5|Hypershell Co., Ltd|
|0x0FC6|ASD Lighting PLC|
|0x0FC7|Pharox B.V.|
|0x0FC8|Eforthink Technology Co., Ltd.|
|0x0FC9|Zirbel Bike GmbH|
|0x0FCA|Legato Audio, Inc.|
|0x0FCB|Biocorp Production|
|0x0FCC|Neurotherapeutics Ltd|
|0x0FCD|Wimate Technology Solutions Pvt Ltd|
|0x0FCE|NOJA Power|
|0x0FCF|Nestlab AS|
|0x0FD0|Stark Future SL|
|0x0FD1|Auranova LLC|
|0x0FD2|Domteknika S.A|
|0x0FD3|YDIIT Co., Ltd|
|0x0FD4|QRITAGYA LLP|



Bluetooth SIG Proprietary 

Page 324 of 442 

**Assigned Numbers** / Document 

**2026-06-23** 

|0x0FD5|Telemacy Ltd|
|---|---|
|0x0FD6|LIOTYS|
|0x0FD7|Hondata, Inc.|
|0x0FD8|SIVA Inotec Limited|
|0x0FD9|REMDEVICE S.R.L.|
|0x0FDA|NIVELCO PROCESS CONTROL CO.|
|0x0FDC|Beijing Hongsi Electronic Technology Co.,Ltd.|
|0x103A|FOSS Analytical A/S|
|0x103D|Blahaj Ltd|
|0x103E|Nextorage Corporation|
|0x103F|infyni|
|0x1040|Raspberry Pi|
|0x1041|Shenzhen Huasheng Jiaye Technology Co., Ltd|
|0x1042|Namara Water Technologies, Inc|
|0x1043|Shanghai Holychip Electronic Co., Ltd.|
|0x1044|Rotarex S.A.|
|0x1045|VOTRONIC Elektronik-Systeme GmbH|
|0x1046|EXELIO S.R.L.|
|0x1047|Yamaha Motor eBike Systems GmbH|
|0x1048|Ferrari s.p.a.|
|0x1049|Anhui Ubico Advanced Manufacture Co., Ltd.|
|0x104A|Mueller-BBM Rail Technologies GmbH|
|0x104B|Yoto Limited|
|0x104C|LAUMAS Elettronica S.r.l.|
|0x104D|BUND MEDIA PTY. LTD.|
|0x104E|LSC Control Systems Pty Ltd|
|0x104F|Safety Lighting Research Pty Ltd|
|0x1050|Hangzhou Sneuro Medical Co., Ltd.|
|0x1051|UnlimitedIRL LLC|
|0x1052|CHOZEN International CO., LTD.|
|0x1053|Sensorworx, Inc.|
|0x1054|Romcor Pty Ltd|
|0x1055|NewNet, Inc.|
|0x1056|CLM-Solutions|
|0x1057|RSA Security LLC|
|0x1058|Gravity (Shenzhen)Space Technology Co., Ltd|



Bluetooth SIG Proprietary 

Page 325 of 442 

**Assigned Numbers** / Document 

**2026-06-23** 

|0x1059|Shenzhen Shuye Technology Co.,Ltd.|
|---|---|
|0x105A|ArjoHuntleigh AB|
|0x105B|Fairbanks Scales Inc.|
|0x105C|YANMAR HOLDINGS CO., LTD.|
|0x105D|Williams Sound, LLC|
|0x105E|Zuuka Limited|
|0x105F|Chip-pump Microelectronics|
|0x1060|TOMOE VALVE CO.,LTD|
|0x1061|VCS Vision Control Solutions GmbH|
|0x1062|Framason Audio S.A|
|0x1063|Field Line Automation Ltd|
|0x1064|Shearwater Research Inc.|
|0x1065|HIGH HIT ENTERPRISE CO., LTD.|
|0x1066|Algiz AS|
|0x1067|Shenzhen FHX Precision Hardware & Plastic Co., Ltd.|
|0x1068|Hotron Co., Ltd.|
|0x1069|Remoticom B.V.|
|0x106A|OSKEY|
|0x106B|GORDON MURRAY GROUP LIMITED|
|0x106C|Global Electronics|
|0x106D|Rokstar Holdings Limited|
|0x106E|STRYDE LIMITED|
|0x106F|Prosaris Solutions Ltd.|
|0x1070|TLV Co.,LTD.|
|0x1071|IZYBAT|
|0x1072|ALTESSA SOLUTIONS INC.|
|0x1073|Hong Kong Future lntelligent Technology Co., Limited|
|0x1074|CenTrak, Inc.|
|0x1075|TM-TECHNOLOGIES, JSC|
|0x1076|ALPHADIF|
|0x1077|Shenzhen Muyu Technology Co., Ltd.|
|0x1078|augment AI Inc.|
|0x1079|Digimax Innovative Products LTD.|
|0x107A|CodaHD LLC|
|0x107B|E.J. Brooks Company|
|0x107C|Martin.Care GmbH|



Bluetooth SIG Proprietary 

Page 326 of 442 

**Assigned Numbers** / Document 

**2026-06-23** 

|0x107D|IMPACT-BZ LTD|
|---|---|
|0x107E|Belford investment holdings pty ltd|
|0x107F|Anton Paar ConsumerTec GmbH|
|0x1080|Knit Sound Company|
|0x1081|Australis Scientific Pty Ltd.|
|0x1082|AIR PRODUCTS AND CHEMICALS, INC.|
|0x1083|SYSTEM LOCO LTD|
|0x1084|24x8, LLC|
|0x1085|FUTURE INTELLIGENCE TECHNOLOGY SINGAPORE PTE. LTD.|
|0x1086|M3SH TECHNOLOGY INC.|
|0x1087|Matisse Interactif inc.|
|0x1088|Sesame AI, Inc.|
|0x1089|DEER MANAGEMENT SYSTEMS, LLC|
|0x108A|Grayhill Inc.|
|0x108B|PERUN TECH SP. Z O.O.|
|0x108C|Tiny Displays Inc.|
|0x108D|DEWINE Labs GmbH|
|0x108E|Industrial Computing Limited|
|0x108F|SkyCell AG|
|0x1090|NAGANO KEIKI CO., LTD.|
|0x1091|COSEL ELEKTRONIK OTOMASYON SISTEMLERI SANAYI VE TICARET<br>LIMITED SIRKETI|
|0x1092|BXB DIGITAL PTY LIMITED|
|0x1093|Mindspire Limited|
|0x1094|Personal Products Co., Ltd.|
|0x1095|CardiaMetrics|
|0x1096|Aktv8 LLC|
|0x1097|Knox Associates, Inc.|
|0x1098|smart-TEC GmbH & Co. KG|
|0x1099|Selco GmbH|
|0x109A|MONNIT CORPORATION|
|0x109B|Tensoli GmbH|
|0x109C|KIBLE|
|0x109D|terumo|
|0x109E|SHIBAURA ELECTRONICS CO., LTD.|
|0x109F|Shenzhen Hali-Power Industrial Co., Ltd.|



Bluetooth SIG Proprietary 

Page 327 of 442 

**Assigned Numbers** / Document 

**2026-06-23** 

|0x10A0|inContAlert GmbH|
|---|---|
|0x10A1|flexlog GmbH|
|0x10A2|Audio Research Corporation|
|0x10A3|BQT Solutions (NZ) Limited|
|0x10A4|RAPIDISE TECHNOLOGY PRIVATE LIMITED|
|0x10A5|Cebreo Medical A/S|
|0x10A6|Lightnovo ApS|
|0x10A7|Pedigree Technologies, LLC|
|0x10A8|Zhejiang Wanyou Intelligent Technology Co., Ltd.|
|0x10A9|Mallow|
|0x10AA|Sigenergy Technology Co., Ltd.|
|0x10AB|ObjectSpectrum, LLC|
|0x10AC|ATsens Co.,Ltd.|
|0x10AD|CoreHW Semiconductor Oy|
|0x10AE|Raycon Inc.|
|0x10AF|Hangzhou SDIC Microelectronics Inc.|
|0x10B0|VSC Synapse LLC|
|0x10B1|Senops Tracker, LLC|
|0x10B2|Shenzhen Xunzong Zhilian Technology Co., Ltd.|
|0x10B3|HOSEOTELNET Co., Ltd.|
|0x10B4|Solabs Technology (HK) Co., Limited|
|0x10B5|SpartanLync Technologies Corp.|
|0x10B6|SHENZHEN CHINA MICRO SEMICON CO.,LIMITED|
|0x10B7|Fosilicon Co., Ltd|
|0x10B8|Palarum LLC|
|0x10B9|CYNC Labs, Inc.|
|0x10BA|Noble HiFi, LLC|
|0x10BB|VERGESENSE INC|
|0x10BC|Harley-Davidson Motor Company|
|0x10BD|Variowell Development GmbH|
|0x10BE|UNIVERSAL REMOTE CONTROL, INC.|
|0x10BF|All Inspire Health, Inc.|
|0x10C0|Minda Corporation Ltd|
|0x10C1|Shenzhen ECO-NEWLEAF CO., LTD|
|0x10C2|SainStore Technology Co., Ltd.|
|0x10C3|TECH FASS s.r.o.|



Bluetooth SIG Proprietary 

Page 328 of 442 

**Assigned Numbers** / Document 

**2026-06-23** 

|0x10C4|OPICA GmbH|
|---|---|
|0x10C5|Cardinal Scale Mfg|
|0x10C6|Piscisea Tech Inc|
|0x10C7|CHIYODA TECHNOL CORPORATION|
|0x10C8|mylife Diabetes Care AG|
|0x10C9|Tractian Technologies Inc.|
|0x10CA|FairPhone B.V|
|0x10CB|The90, Inc.|
|0x10CC|Linde GmbH|
|0x10CD|Kumho Tire|
|0x10CE|Inito Health Inc.|
|0x10CF|Tempur World, LLC|
|0x10D0|Band24, LLC|
|0x10D1|Acer Inc.|
|0x10D2|Hehku Company Oy|
|0x10D3|AGCO Corporation|
|0x10D4|Victor Hasselblad AB|
|0x10D5|IntelliDesign Pty Ltd|
|0x10D6|UltronSMART Inc.|
|0x10D7|Arashi Vision Inc.|
|0x10D8|Westfalen Aktiengesellschaft|
|0x10D9|Liebherr-Hausgeraete GmbH|
|0x10DA|SKAIChips Co.,Ltd|
|0x10DB|SZ Avinox Innovation Co., Ltd.|
|0x10DC|Spectra Precision (Kaiserslautern) GmbH|
|0x10DD|Triumph Designs Ltd|
|0x10DE|Gastron Co.,Ltd|
|0x10DF|Hawkeye Surgical Lighting LLC|
|0x10E0|Arnold & Richter Cine Technik GmbH & Co. Betriebs KG|
|0x10E1|Neurable, Inc|



Bluetooth SIG Proprietary 

Page 329 of 442 

**Assigned Numbers** / Document 

**2026-06-23** 

#### **7.2 Company Identifiers by Name** 

Last Modified: 2026-06-23 

Filename: assigned_numbers/company_identifiers/company_identifiers.yaml 

|**Name**|**Value**|
|---|---|
|10AK Technologies|0x0116|
|11 Health & Technologies Limited|0x0872|
|12mm Health Technology (Hainan) Co., Ltd.|0x0F43|
|1bar.net Limited|0x087E|
|1UP USA.com llc|0x0449|
|2048450 Ontario Inc|0x045A|
|24x8, LLC|0x1084|
|2587702 Ontario Inc.|0x0A37|
|2N TELEKOMUNIKACE a.s.|0x07B3|
|3ALogics, Inc.|0x0C7D|
|3Com|0x0005|
|3DiJoy Corporation|0x0054|
|3DSP Corporation|0x0049|
|3flares Technologies Inc.|0x0331|
|3IWare Co., Ltd.|0x03D9|
|3M|0x02D0|
|3SI Security Systems, Inc|0x0A45|
|4eBusiness GmbH|0x0775|
|4iiii Innovations Inc.|0x0679|
|4MOD Technology|0x04DD|
|701x Inc.|0x0AF3|
|70mai Co.,Ltd.|0x0909|
|8Power Limited|0x0824|
|9313-7263 Quebec inc.|0x0E2C|
|9512-5837 QUEBEC INC.|0x0E1E|
|9974091 Canada Inc.|0x0513|
|A puissance 3|0x0839|
|A&D Engineering, Inc.|0x0069|
|A-Safe Limited|0x03B8|
|A.GLOBAL co.,Ltd.|0x0C8A|
|A.Y. McDonald Mfg. Co.|0x0F8B|
|AAMP of America|0x00BE|



Bluetooth SIG Proprietary 

Page 330 of 442 

**Assigned Numbers** / Document 

**2026-06-23** 

|Aardex Ltd.|0x0BDA|
|---|---|
|AB Electrolux|0x0705|
|ABAX AS|0x0711|
|ABB Inc|0x08B7|
|ABB Oy|0x06BD|
|ABB S.p.A.|0x082E|
|Abbott|0x03BB|
|ABEYE|0x0974|
|Abiogenix Inc.|0x01C7|
|Able Trend Technology Limited|0x018A|
|ABLEPAY TECHNOLOGIES AS|0x09D1|
|ABLIC Inc.|0x0731|
|ABOV Semiconductor|0x02CA|
|Above Average Outcomes, Inc.|0x00EE|
|Absolute Audio Labs B.V.|0x0993|
|ABUS August Bremicker Soehne Kommanditgesellschaft|0x0CC4|
|Accel Semiconductor Ltd.|0x004A|
|Accelerated Systems|0x0B1D|
|Accent Advanced Systems SLU|0x08C0|
|Access Co., Ltd|0x07FB|
|Accumulate AB|0x0156|
|ACE CAD Enterprise Co., Ltd. (ACECAD)|0x03A5|
|Ace Sensor Inc|0x00BC|
|Acer Inc.|0x10D1|
|AceUni Corp., Ltd.|0x00F8|
|ACKme Networks, Inc.|0x0246|
|Aclara Technologies LLC|0x08A0|
|aconno GmbH|0x0C0B|
|ACOS CO.,LTD.|0x085C|
|Acoustic Stream Corporation|0x0194|
|Acromag|0x035F|
|ACS-Control-System GmbH|0x05EA|
|Actev Motors, Inc.|0x0968|
|Actions Technology Co.,Ltd|0x03E0|
|ActiveBlu Corporation|0x0389|
|ACTS Technologies<br>Bluetooth SIG Proprietary|0x00E8<br>P|



Page 331 of 442 

**Assigned Numbers** / Document 

**2026-06-23** 

|Acuity Brands Lighting, Inc|0x0346|
|---|---|
|Acurable Limited|0x05D3|
|Ad Hoc Electronics, llc.|0x0E17|
|adafruit industries|0x0822|
|Adam Hall GmbH|0x0B4D|
|ADATA Technology Co., LTD.|0x08D4|
|AdBabble Local Commerce Inc.|0x0257|
|ADC Technology, Inc.|0x0492|
|Addaday|0x0B69|
|Adero, Inc.|0x069A|
|Adevo Consulting AB|0x0AAD|
|ADH GUARDIAN USA LLC|0x0610|
|ADHD Friendly co.Ltd|0x0F16|
|ADHERIUM(NZ) LIMITED|0x05A2|
|adidas AG|0x00C3|
|Adolene, Inc.|0x0530|
|Adolf Wuerth GmbH & Co KG|0x093D|
|ADTRAN, Inc.|0x0C4B|
|Advanced Application Design, Inc.|0x01EA|
|Advanced Electronic Applications, Inc|0x0D2E|
|Advanced Electronic Designs, Inc.|0x054B|
|Advanced PANMOBIL systems GmbH & Co. KG|0x0091|
|Advanced Telemetry Systems, Inc.|0x0663|
|ADVEEZ|0x098E|
|Adventures of the Persistently Impaired (and other tales) Limited|0x0E73|
|AeroScout|0x03A7|
|Aerosens LLC|0x09BC|
|AETERLINK|0x0C0F|
|AEV spol. s r.o.|0x0793|
|Afero, Inc.|0x02D2|
|AFFORDABLE ELECTRONICS INC|0x0574|
|After Technologies AS|0x0E54|
|AG Measurematics Pvt. Ltd.|0x0316|
|AGCO Corporation|0x10D3|
|Agitron d.o.o.|0x0944|
|AGZZX OPTOELECTRONICS TECHNOLOGY CO., LTD|0x0C89|



Bluetooth SIG Proprietary 

Page 332 of 442 

**Assigned Numbers** / Document 

**2026-06-23** 

|AIAIAI ApS|0x0AAF|
|---|---|
|AIC semiconductor (Shanghai) Co., Ltd.|0x0B3B|
|AINA-Wireless Inc.|0x02CB|
|AIR AROMA INTERNATIONAL PTY LTD|0x0D69|
|Air Automotive tracking Inc.|0x0FC1|
|AIR PRODUCTS AND CHEMICALS, INC.|0x1082|
|Air-Weigh|0x0A30|
|Airbly Inc.|0x03B7|
|AirBolt Pty Ltd|0x036D|
|Aireware LLC|0x0135|
|Airgraft Inc.|0x0DA3|
|AiRISTA|0x0928|
|Airoha Technology Corp.|0x07E3|
|Airoha Technology Corp.|0x0094|
|AIRSTAR|0x09FF|
|Airthings ASA|0x0334|
|AirTurn, Inc.|0x0122|
|Airwallet ApS|0x0DA8|
|Aixlink(Chengdu) Co., Ltd.|0x0B77|
|AJP2 Holdings, LLC|0x0337|
|Akciju sabiedriba ”SAF TEHNIKA”|0x0702|
|Akix S.r.l.|0x0D17|
|Aktiebolaget Ebeco|0x0F93|
|Aktiebolaget Regin|0x098F|
|Aktiebolaget Sandvik Coromant|0x07EF|
|Aktv8 LLC|0x1096|
|AL-KO Geraete GmbH|0x0AD7|
|Alango Technologies Ltd|0x06E2|
|Alarm.com Holdings, Inc|0x06C1|
|Alatech Tehnology|0x023A|
|Alban Giacomo S.P.A.|0x0C5B|
|Albertronic BV|0x091A|
|Albrecht JUNG|0x0527|
|AlbynMedical|0x04BD|
|ALCARE Co., Ltd.|0x06EF|
|Alcatel|0x0024|



Bluetooth SIG Proprietary 

Page 333 of 442 

**Assigned Numbers** / Document 

**2026-06-23** 

|ALE International|0x039C|
|---|---|
|ALEX DENKO CO.,LTD.|0x0C0E|
|ALF Inc.|0x0C9A|
|Alfa Laval Corporate AB|0x0B10|
|Alfen ICU B.V.|0x0B91|
|Alflex Products B.V.|0x0860|
|Alfred International Inc.|0x06F3|
|Alfred Kaercher SE & Co. KG|0x07E2|
|Algiz AS|0x1066|
|Algoria|0x037A|
|Alibaba (China) Co., Ltd.|0x0EC5|
|Alif Semiconductor, Inc.|0x0CDD|
|ALIZENT International|0x0A86|
|All Inspire Health, Inc.|0x10BF|
|Allegion|0x013B|
|Allgon AB|0x0E8B|
|Allswell Inc.|0x03AE|
|Allterco Robotics ltd|0x0BA9|
|Almendo Technologies GmbH|0x06E8|
|Alo AB|0x065E|
|ALOTTAZS LABS, LLC|0x029D|
|Alpha Audiotronics, Inc.|0x0353|
|Alpha Nodus, inc.|0x03B4|
|ALPHADIF|0x1076|
|AlphaTheta Corporation|0x0E31|
|Alpine Electronics (China) Co., Ltd|0x0140|
|Alpine Labs LLC|0x03A1|
|Alps Alpine Co., Ltd.|0x0272|
|Alpwise|0x009A|
|Also, Inc.|0x0F78|
|ALT-TEKNIK LLC|0x0588|
|Altaneos|0x080A|
|ALTESSA SOLUTIONS INC.|0x1072|
|Alticor Inc.|0x0431|
|Altina Inc.|0x0E57|
|Altonics|0x0716|



Bluetooth SIG Proprietary 

Page 334 of 442 

**Assigned Numbers** / Document 

**2026-06-23** 

|ALTYOR|0x0259|
|---|---|
|Aluna|0x06E4|
|amadas|0x03BD|
|Amazon.com Services LLC|0x0171|
|ambie|0x0A56|
|Ambient IoT Pty Ltd|0x0D6C|
|Ambient Life Inc.|0x0EE5|
|Ambient Sensors LLC|0x0916|
|Ambimat Electronics|0x0148|
|Ambiq|0x09AC|
|Ambystoma Labs Inc.|0x05B8|
|AMC International Alfa Metalcraft Corporation AG|0x0946|
|American Music Environments|0x02E8|
|American Technology Components, Incorporated|0x0B8B|
|Ameso Tech (OPC) Private Limited|0x0CBA|
|AMETEK, Inc.|0x0AF6|
|AMG Lab LLC|0x0EA6|
|AMICCOM Electronics Corporation|0x00C0|
|Amiko srl|0x0BCD|
|Amina Distribution AS|0x0CE5|
|Amlogic, Inc.|0x0C85|
|Amon Co.Ltd.|0x0F82|
|Amp Fit Israel LTD|0x0F28|
|Ampetronic Ltd|0x0B87|
|Ampler Bikes OU|0x0A00|
|Amplifico|0x0626|
|Amsted Digital Solutions Inc.|0x0688|
|Amtech Systems, LLC|0x074D|
|Amtronic Sverige AB|0x0517|
|Anacove Inc.|0x0F87|
|Analog Devices, Inc.|0x04B7|
|ANC CHINA LIMITED|0x0F09|
|Ancoe Industry Corporation|0x0F69|
|AND!XOR LLC|0x049E|
|Andon Health Co.,Ltd|0x0804|
|Andreas Stihl AG & Co. KG|0x03DD|



Bluetooth SIG Proprietary 

Page 335 of 442 

**Assigned Numbers** / Document 

**2026-06-23** 

|Andrews & Arnold Ltd|0x0E9C|
|---|---|
|Androtec GmbH|0x04FA|
|Angel Medical Systems, Inc.|0x0BF2|
|Anhui Huami Information Technology Co., Ltd.|0x0157|
|Anhui Listenai Co|0x0AAB|
|Anhui Ubico Advanced Manufacture Co., Ltd.|0x1049|
|Anima|0x02CF|
|Animas Corp|0x0271|
|Anker Innovations Limited|0x0CC2|
|Anki Inc.|0x05F8|
|Anloq Technologies Inc.|0x04D2|
|AnotherBrain inc.|0x09C0|
|Anova Applied Electronics|0x05D0|
|Ant Group Co., Ltd.|0x0E60|
|Antitronics Inc.|0x075C|
|Anton Paar ConsumerTec GmbH|0x107F|
|Anton Paar GmbH|0x0990|
|AntTail.com|0x0572|
|ANUME s.r.o.|0x0D74|
|Anyka (Guangzhou) Microelectronics Technology Co, LTD|0x01F8|
|Aperia Technologies, Inc.|0x0BC5|
|Apexar Technologies S.A.|0x0546|
|API-K|0x0A1D|
|Aplix Corporation|0x00BD|
|Apogee Instruments|0x0644|
|Apollogic Sp. z o.o.|0x0A21|
|Appception, Inc.|0x033A|
|Appion Inc.|0x038C|
|Apple, Inc.|0x004C|
|Applied Neural Research Corp|0x05DA|
|Applied Science, Inc.|0x03BE|
|AppNearMe Ltd|0x0373|
|Appside co., ltd.|0x043B|
|Apption Labs Inc.|0x037B|
|Apptricity Corporation|0x0952|
|APT Ltd.|0x004F|



Bluetooth SIG Proprietary 

Page 336 of 442 

**Assigned Numbers** / Document 

**2026-06-23** 

|Aptcode Solutions|0x026F|
|---|---|
|Aptener Mechatronics Private Limited|0x0DCA|
|Aquana, LLC|0x0E52|
|AR Timing|0x0201|
|Arashi Vision Inc.|0x10D7|
|Arblet Inc.|0x05FB|
|Arch Systems Inc.|0x03C0|
|Archon Controls LLC|0x0E7E|
|ARCHOS SA|0x00CF|
|ARCOM|0x084A|
|Ardic Technology|0x02EB|
|ARDUINO SA|0x09A3|
|Arendi AG|0x011D|
|Areus Engineering GmbH|0x029F|
|Argenox Technologies|0x0240|
|ArjoHuntleigh AB|0x105A|
|Arlo Technologies, Inc.|0x0C19|
|ARMATURA LLC|0x0B33|
|Arnold & Richter Cine Technik GmbH & Co. Betriebs KG|0x10E0|
|ARP Devices Limited|0x00A8|
|ART AND PROGRAM, INC.|0x0766|
|ART SPA|0x0B30|
|Arwin Technology Limited|0x0557|
|Asahi Denso Co.,Ltd.|0x0EBB|
|Asahi Kasei Corporation|0x0965|
|ASB Bank Ltd|0x03BC|
|Ascensia Diabetes Care US Inc.|0x0167|
|ASD Lighting PLC|0x0FC6|
|Aseptico, Inc.|0x0EF9|
|Aseptika Ltd|0x027C|
|Askey Computer Corp.|0x08EE|
|Aspenta International|0x0318|
|ASPion GmbH|0x062C|
|ASR Microelectronics (Shanghai) Co., Ltd.|0x0910|
|ASR Microelectronics(ShenZhen)Co., Ltd.|0x0917|
|ASSA ABLOY|0x012E|



Bluetooth SIG Proprietary 

Page 337 of 442 

**Assigned Numbers** / Document 

**2026-06-23** 

|ASTEM Co.,Ltd.|0x0939|
|---|---|
|Asthrea D.O.O.|0x0630|
|Astra LED AG|0x0DCD|
|Astro, Inc.|0x0324|
|Astute Access Group Limited|0x0F56|
|Asustek Computer Inc.|0x0E41|
|ASYSTOM|0x0E13|
|ATANS TECHNOLOGY INC.|0x0FBC|
|ATEGENOS PHARMACEUTICALS INC|0x0DEC|
|Aterica Inc.|0x0386|
|Atheros Communications, Inc.|0x0045|
|Athlos Oy|0x09EA|
|ATLANTIC SOCIETE FRANCAISE DE DEVELOPPEMENT THERMIQUE|0x0D83|
|Atmel Corporation|0x0013|
|Atmosic Technologies, Inc.|0x0A24|
|ATsens Co.,Ltd.|0x10AC|
|atSpiro ApS|0x0C4A|
|Atus BV|0x0109|
|Audeara Pty Ltd|0x09F2|
|Audeze LLC|0x0F35|
|Audi AG|0x010E|
|audifon GmbH & Co. KG|0x0784|
|Audinor ApS|0x0E9D|
|Audio Partnership Plc|0x0B9E|
|Audio Research Corporation|0x10A2|
|Audio-Technica Corporation|0x0618|
|Audiodo AB|0x0710|
|augment AI Inc.|0x1078|
|August Home, Inc|0x01D1|
|AUMOVIO Systems, Inc.|0x004B|
|Auranova LLC|0x0FD1|
|Aurea Solucoes Tecnologicas Ltda.|0x07AE|
|Austco Communication Systems|0x00D5|
|Australis Scientific Pty Ltd.|0x1081|
|AUTEC Gesellschaft fuer Automationstechnik mbH|0x09B5|
|AuthAir, Inc<br>Bluetooth SIG Proprietary|0x02F3<br>P|



Page 338 of 442 

**Assigned Numbers** / Document 

**2026-06-23** 

|AuthGate B.V.|0x0EC6|
|---|---|
|Authomate Inc|0x0404|
|AUTHOR-ALARM, razvoj in prodaja avtomobilskih sistemov proti kraji, d.o.o.|0x0DDC|
|Automated Pet Care Products, LLC|0x0C0A|
|Automatic Labs, Inc.|0x06DB|
|Automation Components, Inc.|0x033F|
|Automotive Data Solutions Inc|0x0488|
|Autonet Mobile|0x007F|
|Auxivia|0x04EE|
|Avack Oy|0x04E5|
|Avago Technologies|0x004E|
|Avanos Medical, Inc.|0x0FA5|
|Avaya Inc.|0x06E0|
|Avedis Zildjian Co.|0x0E24|
|Avempace SARL|0x0552|
|Averos FZCO|0x04BE|
|Avetos Design LLC|0x0E8F|
|Avi-on|0x01C9|
|Avid Identification Systems, Inc.|0x05DB|
|Avivomed Inc.|0x0FB1|
|AVM Berlin|0x001F|
|Avvel International|0x03C8|
|AW Company|0x06D8|
|AwoX|0x0160|
|AXELIFE|0x0B80|
|Axentia Technologies AB|0x07BE|
|Axes System sp. z o. o.|0x0508|
|Axiomware Systems Incorporated|0x05A3|
|Axis Communications AB|0x0D5B|
|AXTRO PTE. LTD.|0x0A77|
|Ayatan Sensors|0x02C6|
|Ayxon-Dynamics GmbH|0x0A02|
|Azbil Co.|0x080D|
|B&W Group Ltd.|0x014F|
|B.E.A. S.A.|0x0B0F|
|B.E.G. Brueck Electronic GmbH|0x0FA3|



Bluetooth SIG Proprietary 

Page 339 of 442 

**Assigned Numbers** / Document 

**2026-06-23** 

|Backbone Labs, Inc.|0x0421|
|---|---|
|Badass eBikes GmbH|0x0F7F|
|Badger Meter|0x0D32|
|Baidu|0x011C|
|Band Industries, inc.|0x096E|
|Band XI International, LLC|0x0064|
|Band24, LLC|0x10D0|
|BandSpeed, Inc.|0x0020|
|Bang & Olufsen A/S|0x0103|
|Baracoda Daily Healthtech.|0x0B26|
|Barnacle Systems Inc.|0x07CC|
|Barnes Group Inc.|0x0AC6|
|Barrot Technology Co.,Ltd.|0x08E7|
|Bartec Auto Id Ltd|0x0D04|
|BASIC MICRO.COM,INC.|0x03ED|
|BatAndCat|0x017D|
|Battery-Biz Inc.|0x07BA|
|Baxter Healthcare Corporation|0x0C94|
|Bayerische Motoren Werke AG|0x05EB|
|BBC Bircher AG|0x0EED|
|BBPOS Limited|0x02AB|
|BD Medical|0x042A|
|BDE Technology Co., Ltd.|0x00B4|
|Be Interactive Co., Ltd|0x0708|
|Beaconzone Ltd|0x06BA|
|Beam Labs, LLC|0x0622|
|Beamex Oy Ab|0x0D91|
|Beats Electronics|0x00CC|
|Beautiful Enterprise Co., Ltd.|0x006C|
|Becker Antriebe GmbH|0x0811|
|Beco, Inc|0x057E|
|Becon Technologies Co.,Ltd.|0x03F9|
|BEEHERO, INC.|0x0C2F|
|BEEPINGS|0x0DD5|
|BeerTech LTD|0x083C|
|Beflex Inc.|0x06B9|



Bluetooth SIG Proprietary 

Page 340 of 442 

**Assigned Numbers** / Document 

**2026-06-23** 

|BEGA Gantenbrink-Leuchten KG|0x046D|
|---|---|
|Beghelli Spa|0x0564|
|Beidou Intelligent Connected Vehicle Technology Co., Ltd.|0x0DFB|
|Beijing Big Moment Technology Co., Ltd.|0x08EB|
|Beijing CarePulse Electronic Technology Co, Ltd|0x023B|
|BEIJING ELECTRIC VEHICLE CO.,LTD|0x09A6|
|Beijing ESWIN Computing Technology Co., Ltd.|0x0B1F|
|Beijing Hao Heng Tian Tech Co., Ltd.|0x073D|
|Beijing HC-Infinite Technology Limited|0x0A46|
|Beijing Hongsi Electronic Technology Co.,Ltd.|0x0FDC|
|Beijing Jingdong Century Trading Co., Ltd.|0x0703|
|Beijing Pinecone Electronics Co.,Ltd.|0x05B7|
|Beijing ranxin intelligence technology Co.,LTD|0x0BB1|
|BeiJing SmartChip Microelectronics Technology Co.,Ltd|0x0D7C|
|Beijing Spring Creation Technology Co., Ltd.|0x0F20|
|Beijing SuperHexa Century Technology CO. Ltd|0x0A67|
|Beijing Unisoc Technologies Co., Ltd.|0x073F|
|Beijing Winner Microelectronics Co.,Ltd|0x070C|
|Beijing Wisepool Infinite Intelligence Technology Co.,Ltd|0x0B9A|
|Beijing Zero Zero Infinity Technology Co.,Ltd.|0x09F3|
|BeiJing ZiJie TiaoDong KeJi Co.,Ltd.|0x0B24|
|Beijing Zizai Technology Co., LTD.|0x0900|
|beken|0x05F0|
|Bekey A/S|0x00B2|
|Belford investment holdings pty ltd|0x107E|
|Belkin International, Inc.|0x005C|
|BELLDESIGN Inc.|0x0C3E|
|Bellman & Symfon Group AB|0x0464|
|Belun Technology Company Limited|0x0B0A|
|Belusun Technology Ltd.|0x0DF9|
|Benchmark Drives GmbH & Co. KG|0x04FB|
|benegear, inc.|0x0558|
|Benning Elektrotechnik und Elektronik GmbH & Co. KG|0x041D|
|Berlinger & Co. AG|0x0C43|
|Bernard Krone Holding SE & Co.KG|0x0981|
|Berner International LLC|0x0875|



Bluetooth SIG Proprietary 

Page 341 of 442 

**Assigned Numbers** / Document 

**2026-06-23** 

|BeSpoon|0x0674|
|---|---|
|Bestechnic(Shanghai),Ltd|0x02B0|
|betternotstealmybike UG (with limited liability)|0x08CF|
|Beurer GmbH|0x0611|
|bewhere inc|0x0277|
|Beyerdynamic GmbH & Co. KG|0x0D81|
|bf1systems limited|0x08BD|
|bGrid B.V.|0x098C|
|BH SENS|0x0B35|
|BH Technologies|0x0DFD|
|bHaptics Inc.|0x0D3D|
|BHClears Microelectronics (Shanghai) Co., Ltd.|0x0EAE|
|BHM-Tech Produktionsgesellschaft m.b.H|0x0988|
|BIA NEUROSCIENCE INC.|0x0FA2|
|Biceek Inc|0x0F8E|
|Big Kaiser Precision Tooling Ltd|0x0992|
|BIGBEN|0x0CA0|
|BIGREDBEE, LLC|0x0F66|
|BikeFinder AS|0x056F|
|Binauric SE|0x00CB|
|Biocorp Production|0x0FCB|
|BioEchoNet inc.|0x095E|
|Biogents AG|0x0F6C|
|BioIntelliSense, Inc.|0x08FD|
|Bioliberty Ltd|0x0D18|
|BioMech Sensor LLC|0x0365|
|Biomedical Research Ltd.|0x015B|
|Biometrika d.o.o.|0x086D|
|Bionic Avionics Inc.|0x09DB|
|BioResearch Associates|0x00EC|
|BIOROWER Handelsagentur GmbH|0x04AF|
|BiosBob.Biz|0x0B4C|
|Biotechware SRL|0x084B|
|BioTex, Inc.|0x04A8|
|BIOTRONIK SE & Co. KG|0x0979|
|Biovigil<br>Bluetooth SIG Proprietary|0x098A<br>P|



Page 342 of 442 

**Assigned Numbers** / Document 

**2026-06-23** 

|Biovotion AG|0x0624|
|---|---|
|Biowatch SA|0x05CF|
|Biral AG|0x07C0|
|Bird Home Automation GmbH|0x04EB|
|BIROTA|0x07FE|
|Bison Group Ltd.|0x01DF|
|Bitcraze AB|0x01C5|
|BiTECH Automotive (Wuhu) Co.,Ltd|0x0EA4|
|BitGreen Technolabz (OPC) Private Limited|0x0C71|
|Bitkey Inc.|0x072B|
|Bitsplitters GmbH|0x00EF|
|Bitstrata Systems Inc.|0x0350|
|Bitwards Oy|0x0836|
|BIXOLON CO.,LTD|0x0A23|
|Bkon Connect|0x0143|
|BLACK BOX NETWORK SERVICES INDIA PRIVATE LIMITED|0x0D67|
|BlackBerry Limited|0x003C|
|Blackrat Software|0x02F0|
|Blahaj Ltd|0x103D|
|Bleb Technology srl|0x0668|
|Blecon Ltd|0x0E1F|
|Blincam, Inc.|0x04D7|
|BLINQY|0x09B1|
|Blippit AB|0x082F|
|BLITZ electric motors. LTD|0x0C69|
|Blocks Wearables Ltd.|0x0435|
|BLOKS GmbH|0x0396|
|BluDotz Ltd|0x017E|
|Blue Bite|0x0293|
|Blue Clover Devices|0x024C|
|Blue Maestro Limited|0x0133|
|Blue Peacock GmbH|0x0A0D|
|Blue Sky Scientific, LLC|0x0339|
|Blue Sky Scientific, LLC|0x029C|
|Blue Spark Technologies|0x0477|
|Blue Speck Labs, LLC<br>Bluetooth SIG Proprietary|0x021A<br>P|



Page 343 of 442 

**Assigned Numbers** / Document 

**2026-06-23** 

|Blueair AB|0x060E|
|---|---|
|BLUEFIN DATA, LLC|0x0EB4|
|Bluegiga|0x0047|
|BlueID GmbH|0x0C5E|
|BlueIOT(Beijing) Technology Co.,Ltd|0x0975|
|BlueKitchen GmbH|0x048F|
|Bluenetics GmbH|0x07C6|
|Bluepack S.R.L.|0x073E|
|BLUEPROVIDERZ LLC|0x0EFB|
|BlueRadios, Inc.|0x0085|
|BlueStreak IoT, LLC|0x09CF|
|BLUETICKETING SRL|0x0A39|
|Bluetooth SIG, Inc|0x003F|
|Bluetrum Technology Co.,Ltd|0x0642|
|BlueUp|0x0859|
|BlueX Microelectronics Corp Ltd.|0x09B3|
|BluPeak|0x0B0C|
|Blur Product Development|0x02DF|
|BluStor PMC, Inc.|0x0387|
|BM innovations GmbH|0x032D|
|BM3|0x0645|
|BMA ergonomics b.v.|0x02CD|
|Bobrick Washroom Equipment, Inc.|0x081C|
|BodyPlus Technology Co.,Ltd|0x06F8|
|Bodyport Inc.|0x0568|
|Boehringer Ingelheim Vetmedica GmbH|0x08B6|
|BOLTT Sports technologies Private limited|0x0366|
|Bonsai Systems GmbH|0x0462|
|BONX INC.|0x0F41|
|BORA - Vertriebs GmbH & Co KG|0x0B56|
|Borda Technology|0x0AEA|
|BOS Balance of Storage Systems AG|0x0CF5|
|Bose Corporation|0x009E|
|Boss Audio|0x0ADD|
|Boston Scientific Corporation|0x045D|
|Bouffalo Lab (Nanjing)., Ltd.<br>Bluetooth SIG Proprietary|0x0A38<br>P|



Page 344 of 442 

**Assigned Numbers** / Document 

**2026-06-23** 

|Bout Labs, LLC|0x09B7|
|---|---|
|Boxyz, Inc.|0x0D9B|
|BPW Bergische Achsen Kommanditgesellschaft|0x083A|
|BQN|0x0B74|
|BQT Solutions (NZ) Limited|0x10A3|
|BRADATECH Corp.|0x01C1|
|Brady Worldwide Inc.|0x066A|
|Bragi GmbH|0x0241|
|Braveheart Wireless, Inc.|0x0913|
|BRControls Products BV|0x036B|
|Breakwall Analytics, LLC|0x0582|
|BREATHINGS Co., Ltd.|0x0931|
|Breezi.io, Inc.|0x0B4F|
|Breville Group|0x0955|
|BrickXter GmbH|0x0EA7|
|Brightway Innovation Intelligent Technology (Suzhou) Co., Ltd.|0x0F0B|
|Brilliant Home Technology, Inc.|0x0820|
|BRITZ INTERNATIONAL CO.,LTD|0x0F2E|
|BRK Brands, Inc.|0x06B0|
|Broadcom Corporation|0x000F|
|Bronkhorst High-Tech B.V.|0x0995|
|Brookfield Equinox LLC|0x0511|
|Brooksee, Inc.|0x0E4D|
|Brose Verwaltung SE, Bamberg|0x0C45|
|Brother Industries, Ltd|0x0755|
|Brudden|0x0E89|
|Bruel & Kjaer Sound & Vibration|0x07A9|
|BSH|0x01CF|
|BubblyNet, LLC|0x0788|
|BUCHI Labortechnik AG|0x0609|
|Build With Robots Inc.|0x0BB6|
|Bull Group Company Limited|0x0712|
|BUND MEDIA PTY. LTD.|0x104D|
|Bunn-O-Matic Corporation|0x0BA1|
|Burkert Werke GmbH & Co. KG|0x0D26|
|Busch Jaeger Elektro GmbH<br>Bluetooth SIG Proprietary|0x04E9<br>P|



Page 345 of 442 

**Assigned Numbers** / Document 

**2026-06-23** 

|Busch Systems International Inc.|0x0A43|
|---|---|
|Buzz Products Ltd.|0x0671|
|BXB DIGITAL PTY LIMITED|0x1092|
|BYD Company Limited|0x0C34|
|BYSTAMP|0x087B|
|Bytestorm Ltd.|0x02E4|
|Byton North America Corporation|0x07F5|
|C Technologies|0x0026|
|C-MAX Asia Limited|0x0774|
|C. & E. Fein GmbH|0x0996|
|C.Ed. Schulte GmbH Zylinderschlossfabrik|0x0D0D|
|C.G. Air Systemes Inc.|0x0D19|
|C.O.B.O. SpA|0x092F|
|C2 Development, Inc.|0x029B|
|CACI Technologies|0x0ACF|
|CAEN RFID srl|0x00AA|
|Caire Inc.|0x0D99|
|caive Inc.|0x0F0F|
|California Things Inc.|0x070F|
|Calpeda S.p.A.|0x0F83|
|Cambridge Animal Technologies Ltd|0x0A18|
|CAME S.p.A.|0x06C0|
|Candura Instruments|0x03A2|
|CANDY HOUSE, Inc.|0x055A|
|Canon Electronics Inc.|0x0D8F|
|Canon Finetech Nisca Inc.|0x0A9D|
|Canon Inc.|0x01A9|
|Canopy Growth Corporation|0x0856|
|Canopy Growth Corporation|0x0835|
|Canyon Bicycles GmbH|0x0EC7|
|Capacite|0x0FAE|
|Capetech|0x0950|
|CAPTAIN BLINK|0x0EF2|
|Capte B.V.|0x0E1D|
|CAPTEMP, LDA|0x0EE4|
|CardiaMetrics|0x1095|



Bluetooth SIG Proprietary 

Page 346 of 442 

**Assigned Numbers** / Document 

**2026-06-23** 

|Cardinal Scale Mfg|0x10C5|
|---|---|
|CARDIOID - TECHNOLOGIES, LDA|0x0C80|
|Cardo Systems, Ltd|0x06A1|
|Care Bloom, LLC|0x0AA2|
|Carefree Scott Fetzer Co Inc|0x054C|
|CAREL INDUSTRIES S.P.A.|0x05B2|
|Caresix Inc.|0x0C2A|
|Carestream Dental LLC|0x0A92|
|CareView Communications, Inc.|0x039D|
|Carewear Corp.|0x0709|
|Carmanah Technologies Corp.|0x02E3|
|CARMATE MFG.CO.,LTD|0x05AE|
|Carol Cole Company|0x084E|
|Carrier Corporation|0x0C81|
|Casambi Technologies Oy|0x03C3|
|CASIO COMPUTER CO., LTD.|0x0178|
|Catalyft Labs, Inc.|0x093E|
|Catapult Group International Ltd|0x0C1A|
|Caterpillar Inc|0x01E3|
|CATEYE Co., Ltd.|0x0D0A|
|Cear, Inc.|0x0D9D|
|Cebreo Medical A/S|0x10A5|
|Cedarware, Corp.|0x0D22|
|Celebrities Management Private Limited|0x0E8D|
|CELLCONTROL, INC.|0x0B5E|
|Cello Hill, LLC|0x0986|
|Cennox Group Limited|0x0D21|
|Center ID Corp.|0x052F|
|CenTrak, Inc.|0x1074|
|Centre Suisse d’Electronique et de Microtechnique SA|0x09BD|
|Centrica Connected Home|0x0487|
|Centromere Holding B.V.|0x0F26|
|CeoTronics AG|0x06D2|
|Cerebrum Sensor Technologies Inc.|0x0AFC|
|Cerevast Medical|0x04F3|
|Cerevo|0x0268|



Bluetooth SIG Proprietary 

Page 347 of 442 

**Assigned Numbers** / Document 

**2026-06-23** 

|Ceridwen Limited|0x0EDD|
|---|---|
|Ceruus|0x019E|
|Cesar Systems Ltd.|0x07F7|
|CESYS Gesellschaft für angewandte Mikroelektronik mbH|0x0E37|
|CFLAB TEKNOLOJI TICARET LIMITED SIRKETI|0x0E20|
|Chandler Systems Inc.|0x073A|
|Changsha JEMO IC Design Co.,Ltd|0x0751|
|Changzhou Yongse Infotech Co., Ltd.|0x012A|
|Channel Enterprises (HK) Ltd.|0x01A0|
|CHAR-BROIL, LLC|0x0BDB|
|Chargelib|0x02A9|
|Chargifi Limited|0x0423|
|CHARGTRON IOT PRIVATE LIMITED|0x0C78|
|Chargy Technologies, SL|0x06F0|
|Check Technology Solutions LLC|0x08B8|
|ChefSteps, Inc.|0x0159|
|Chemtronics|0x02BD|
|Chengdu Aich Technology Co.,Ltd|0x0AC7|
|Chengdu Ambit Technology Co., Ltd.|0x0AFA|
|Chengdu CSCT Microelectronics Co., Ltd.|0x0DF8|
|Chengdu Synwing Technology Ltd|0x01CD|
|Cherry GmbH|0x0687|
|Chess Wise B.V.|0x09D0|
|CHEVALIER TECH LIMITED|0x0E82|
|Chicony Electronics Co., Ltd.|0x0108|
|Chip-ing AG|0x03E4|
|Chip-pump Microelectronics|0x105F|
|CHIPOLO d.o.o.|0x08C3|
|Chipsea Technologies (ShenZhen) Corp.|0x06A7|
|CHIYODA TECHNOL CORPORATION|0x10C7|
|CHOZEN International CO., LTD.|0x1052|
|Chromatic Inc.|0x0E98|
|Chrono Therapeutics|0x02B8|
|CHUO Electronics CO., LTD.|0x0317|
|Church & Dwight Co., Inc|0x028F|
|CIMTechniques, Inc.<br>Bluetooth SIG Proprietary|0x07C3<br>P|



Page 348 of 442 

**Assigned Numbers** / Document 

**2026-06-23** 

|Cinetix|0x00AF|
|---|---|
|Circular|0x0E7B|
|Circus World Displays Limited|0x0CDB|
|Ciright|0x0146|
|Cirrus Research plc|0x0DC2|
|Cisco Systems, Inc|0x021B|
|Citizen Holdings Co., Ltd.|0x02EE|
|CLABER S.P.A.|0x02B3|
|Clarinox Technologies Pty. Ltd.|0x00B3|
|Clarion Co. Inc.|0x012F|
|Clarius Mobile Health Corp.|0x02FB|
|Classified Cycling|0x0C3C|
|CLB B.V.|0x097D|
|CleanBands Systems Ltd.|0x0BFA|
|CleanSpace Technology Pty Ltd|0x0B67|
|Clearity, LLC|0x05F4|
|Cleer Limited|0x0A80|
|CLEIO Inc.|0x0CC1|
|CLEVER LOGGER TECHNOLOGIES PTY LIMITED|0x0E12|
|Cleveron AS|0x0A01|
|CLIM8 LIMITED|0x053E|
|CliniCloud Inc|0x0291|
|CLINK|0x03F0|
|ClipsClips LLC|0x0F2D|
|CLM-Solutions|0x1056|
|Closed Joint Stock Company NVP BOLID|0x0D6F|
|Cloudleaf, Inc|0x0192|
|Clover Network, Inc.|0x0721|
|CME PTE. LTD.|0x0825|
|CO-AX Technology, Inc.|0x040F|
|COBI GmbH|0x0338|
|Coburn Technology, LLC|0x08AB|
|Cochlear Bone Anchored Solutions AB|0x01BB|
|Cochlear Limited|0x0497|
|CodaHD LLC|0x107A|
|Code Blue Communications|0x0583|



Bluetooth SIG Proprietary 

Page 349 of 442 

**Assigned Numbers** / Document 

**2026-06-23** 

|Code Corporation|0x01D8|
|---|---|
|Code Gears LTD|0x028B|
|code-Q|0x08DD|
|Codecoup sp. z o.o. sp. k.|0x05C4|
|Codefabrik GmbH|0x0ADA|
|Codegate Ltd|0x010A|
|Codenex Oy|0x0467|
|Codie LLC|0x0EC8|
|CODIUM|0x0AC4|
|Cognosos, Inc.|0x0852|
|Coiler Corporation|0x043D|
|Cokiya Incorporated|0x019C|
|Colorfy, Inc.|0x009C|
|Comarch SA|0x0224|
|CombiQ AB|0x0A1E|
|Combustion, LLC|0x09C7|
|COMELIT GROUP S.P.A.|0x0C20|
|Comm-N-Sense Corp DBA Verigo|0x03AF|
|Commil Ltd|0x0033|
|Comodule GMBH|0x020F|
|Companion Medical, Inc.|0x048E|
|CompanyDeep Ltd|0x0DB9|
|Computer Access Technology Corporation (CATC)|0x0034|
|Computime International Ltd|0x0AAA|
|Comtel Systems Ltd.|0x0BE3|
|conbee GmbH|0x07A8|
|Conexant Systems Inc.|0x001C|
|Confidex|0x09FC|
|connectBlue AB|0x0071|
|Connected Yard, Inc.|0x02E7|
|ConnecteDevice Ltd.|0x0097|
|Conneqtech B.V.|0x0833|
|Connovate Technology Private Limited|0x0172|
|CONSORCIO TRUST CONTROL - NETTEL|0x0C6A|
|CONSTRUKTS, INC.|0x0C07|
|Consumer Sleep Solutions LLC<br>Bluetooth SIG Proprietary|0x0570<br>P|



Page 350 of 442 

**Assigned Numbers** / Document 

**2026-06-23** 

|Contec Medical Systems Co., Ltd.|0x063C|
|---|---|
|ContiTech Deutschland GmbH|0x0F54|
|CONTRINEX S.A.|0x03CE|
|Control Solutions LLC|0x0C41|
|Control-J Pty Ltd|0x0472|
|Controlid Industria, Comercio de Hardware e Servicos de Tecnologia Ltda|0x0884|
|Convergence Systems Limited|0x04F8|
|CONVERTRONIX TECHNOLOGIES AND SERVICES LLP|0x0B57|
|CONWISE Technology Corporation Ltd|0x0042|
|CONZUMEX INDUSTRIES PRIVATE LIMITED|0x0B34|
|Cool Webthings Limited|0x01F5|
|Cooler Pro, LLC|0x0D2D|
|Copeland Cold Chain LP|0x016A|
|CORAL-TAIYI Co. Ltd.|0x0BED|
|Coravin, Inc.|0x0650|
|CORE CORPORATION|0x097A|
|Core Devices LLC|0x0EEA|
|core sensing GmbH|0x0CF8|
|CORE TRANSPORT TECHNOLOGIES NZ LIMITED|0x0566|
|CoreHW Semiconductor Oy|0x10AD|
|COREIOT PTY LTD|0x0719|
|CORE|vision BV|0x08B1|
|Coroflo Limited|0x0BDD|
|CoroWare Technologies, Inc|0x020C|
|CORROHM|0x0C18|
|Corsair|0x0A82|
|CORVENT MEDICAL, INC.|0x0CE2|
|Corvex Connected Safety|0x0771|
|COSEL ELEKTRONIK OTOMASYON SISTEMLERI SANAYI VE TICARET<br>LIMITED SIRKETI|0x1091|
|Cosmed s.r.l.|0x0C10|
|Cosonic Intelligent Technologies Co., Ltd.|0x0FBA|
|Countrymate Technology Limited|0x0964|
|Courtney Thorne Limited|0x033B|
|Cousins and Sears LLC|0x0E36|
|COWBELL ENGINEERING CO.,LTD.|0x08EA|



Bluetooth SIG Proprietary 

Page 351 of 442 

**Assigned Numbers** / Document 

**2026-06-23** 

|COWBOY|0x05E8|
|---|---|
|Coyotta|0x09D7|
|CPAC Systems AB|0x0D41|
|CPS AS|0x09E4|
|CRADERS,CO.,LTD|0x0AF1|
|Crane Payment Innovations, Inc.|0x0D6B|
|Creative Technology Ltd.|0x0076|
|Crescent NV|0x0E33|
|CRESCO Wireless, Inc.|0x0532|
|Crestron Electronics, Inc.|0x04BA|
|Cricut, Inc.|0x0792|
|CRONO CHIP, S.L.|0x07B5|
|Cronologics Corporation|0x037C|
|CRONUS ELECTRONICS LTD|0x0801|
|Crookwood|0x0635|
|CROSS BRAIN CO.,Ltd.|0x0F77|
|Crosscan GmbH|0x087C|
|Crossdoor|0x0EC1|
|Crowd Connected Ltd|0x08E5|
|CrowdGlow Ltd|0x09C9|
|Crownstone B.V.|0x038E|
|CROXEL, INC.|0x0A14|
|Crystal Alarm AB|0x00FA|
|CS INSTRUMENTS GmbH & Co.KG|0x0ED7|
|CSIRO|0x0A51|
|CSR Building Products Limited|0x04B9|
|CST ELECTRONICS (PROPRIETARY) LIMITED|0x079B|
|CTEK Sweden AB|0x097B|
|CUBE TECHNOLOGIES|0x03EE|
|CUBETECH s.r.o.|0x019B|
|Cucumber Lighting Controls Limited|0x0CB7|
|Cue|0x0777|
|Culligan International Company|0x0EA1|
|Cumulus Digital Systems, Inc|0x08EF|
|Curie Point AB|0x0691|
|Currant, Inc.<br>Bluetooth SIG Proprietary|0x029A<br>P|



Page 352 of 442 

**Assigned Numbers** / Document 

**2026-06-23** 

|Current Lighting Solutions LLC|0x0897|
|---|---|
|CV. NURI TEKNIK|0x0CED|
|CVS Health|0x019D|
|Cyber Transport Control GmbH|0x0776|
|CYBERDYNE Inc.|0x0C59|
|Cybex GmbH|0x078D|
|CycLock|0x0D38|
|Cyclops Marine Ltd|0x0CCA|
|CYNC Labs, Inc.|0x10B9|
|Cypress Semiconductor|0x0131|
|CZV, Inc|0x0F65|
|D&M Holdings Inc.|0x0531|
|D-Link Corp.|0x083F|
|Dai Nippon Printing Co., Ltd.|0x0255|
|Daihatsu Motor Co., Ltd.|0x0D7E|
|DAIICHIKOSHO CO., LTD.|0x0A22|
|Daikin Industries, LTD|0x0E65|
|Dairy Tech, LLC|0x0732|
|DaisyWorks, Inc|0x04EF|
|DAKATECH|0x085A|
|DALI Alliance|0x0AE4|
|Dallas Logic Corporation|0x04A7|
|Danfoss A/S|0x042F|
|Danlers Ltd|0x00E1|
|Darkglass Electronics Oy|0x08FB|
|Dash Robotics|0x02C0|
|DashLogic, Inc.|0x09AB|
|Data Sciences International|0x0BA0|
|Dataflow Systems Limited|0x059B|
|Datalogic S.r.l.|0x0D07|
|Datalogic USA, Inc.|0x0D08|
|DATAMARS, Inc.|0x0903|
|DATANG SEMICONDUCTOR TECHNOLOGY CO.,LTD|0x0A60|
|DDS, Inc.|0x0264|
|Deako|0x0C97|
|DECATHLON SE|0x0B18|



Bluetooth SIG Proprietary 

Page 353 of 442 

**Assigned Numbers** / Document 

**2026-06-23** 

|Deep and Steep LLC|0x0F11|
|---|---|
|Deepfield Connect GmbH|0x0BE4|
|DEER MANAGEMENT SYSTEMS, LLC|0x1089|
|DEFA AS|0x027B|
|deister electronic GmbH|0x099C|
|Deity Acoustic Technology Co.|0x0EBE|
|DEKA Research & Development Corp.|0x026B|
|DEKRA TESTING AND CERTIFICATION, S.A.U.|0x0980|
|DELABIE|0x0A3D|
|Dell Computer Corporation|0x041E|
|DeLorme Publishing Company, Inc.|0x0080|
|Delphi Corporation|0x00FC|
|DelpSys, s.r.o.|0x0A8C|
|DELSEY SA|0x0422|
|Delta Cycle Corporation|0x0A75|
|Delta Development Team, Inc|0x0D2F|
|Delta Electronics, Inc.|0x069E|
|Delta Faucet Company|0x0DF5|
|Delta Systems, Inc|0x02EC|
|Delta T Corporation|0x045C|
|Demant A/S|0x0107|
|Dendro Technologies, Inc.|0x0DE7|
|Dension Elektronikai Kft.|0x0871|
|Density Inc.|0x03D1|
|DENSO AIRCOOL CORPORATION|0x0C29|
|Denso Corporation|0x08EC|
|DENSO TEN Limited|0x010D|
|Deone (Shanghai) Communication & Technology Co., Ltd|0x0E49|
|Derichs GmbH|0x0587|
|Dermal Photonics Corporation|0x08F6|
|Dermalapps, LLC|0x0684|
|desamisCo.,Ltd.|0x0F4B|
|Detect Blue Limited|0x051E|
|Deublin Company, LLC|0x09B0|
|DEV TECNOLOGIA INDUSTRIA, COMERCIO E MANUTENCAO DE<br>EQUIPAMENTOS LTDA. - ME|0x0486|



Bluetooth SIG Proprietary 

Page 354 of 442 

**Assigned Numbers** / Document 

**2026-06-23** 

|Devdata S.r.l.|0x030D|
|---|---|
|Develco Products A/S|0x046F|
|Devialet SA|0x0258|
|DeviceDrive AS|0x0782|
|DeVilbiss Healthcare LLC|0x0A1F|
|DeWalch Technologies, Inc.|0x02DC|
|DewertOkin GmbH|0x066B|
|DEWINE Labs GmbH|0x108D|
|DEXATEK Technology LTD|0x0DE6|
|Dexcom, Inc.|0x00D0|
|DEZINE GROUP LLC|0x0F99|
|DHL|0x0DC5|
|Diagnoptics Technologies|0x04B6|
|Diamond Kinetics, Inc.|0x061E|
|DIAODIAO (Beijing) Technology Co., Ltd.|0x0749|
|DIC Corporation|0x0AA3|
|DIG Corporation|0x06CD|
|Digi International Inc (R)|0x02DB|
|Digianswer A/S|0x000C|
|Digibale Pty Ltd|0x079D|
|Digimax Innovative Products LTD.|0x1079|
|DIGISINE ENERGYTECH CO. LTD.|0x07F3|
|Digital Matter Pty Ltd|0x064F|
|DIM3|0x081B|
|Direct Communication Solutions, Inc.|0x07F9|
|Dish Network LLC|0x0640|
|DiUS Computing Pty Ltd|0x0734|
|DIVAN TRADING CO., LTD.|0x0D52|
|DiveNav, Inc.|0x0313|
|Divesoft s.r.o.|0x0BCB|
|DJO Global|0x01F6|
|Djup AB|0x0DB5|
|Dmac Mobile Developments, LLC|0x076A|
|DME Microelectronics|0x01C4|
|Dmet Products Corp.|0x04C2|
|DML LLC|0x0742|



Bluetooth SIG Proprietary 

Page 355 of 442 

**Assigned Numbers** / Document 

**2026-06-23** 

|DNANUDGE LIMITED|0x0847|
|---|---|
|Dodam Enersys Co., Ltd|0x0BFB|
|Dodge Industrial, Inc.|0x0B3C|
|Dolby Labs|0x0309|
|DOM Sicherheitstechnik GmbH & Co. KG|0x04CF|
|Dometic Corporation|0x0845|
|Domintell s.a.|0x0803|
|Domster Tadeusz Szydlowski|0x026C|
|Domteknika S.A|0x0FD2|
|DONGGUAN HELE ELECTRONICS CO., LTD|0x071E|
|Dongguan Liesheng Electronic Co.Ltd|0x071F|
|Dongguan SmartAction Technology Co.,Ltd.|0x06CC|
|Dongguan Trangjan Industrial Co., Ltd|0x0EA8|
|Dongguan Yougo Electronics Co.,Ltd.|0x0CE8|
|Dongguang Huasoo Automation Technology Company Limited|0x0F95|
|donutrobotics Co., Ltd.|0x0A03|
|Dopple Technologies B.V.|0x0849|
|Doppler Lab|0x02AA|
|DORAN MFG. LLC|0x0ED8|
|dormakaba Holding AG|0x0C64|
|Doro AB|0x0E04|
|DOTT Limited|0x0219|
|doubleO Co., Ltd.|0x0D06|
|Douglas Dynamics L.L.C.|0x0ADF|
|Douglas Lighting Controls Inc.|0x0911|
|Down Range Systems LLC|0x0840|
|DP IOT|0x0FA0|
|DPTechnics|0x05F6|
|Draegerwerk AG & Co. KGaA|0x04BC|
|Dragonchip Limited|0x069B|
|Dragonfly Energy Corp.|0x0C9F|
|Drayson Technologies (Europe) Limited|0x0436|
|Dream Devices Technologies Oy|0x05A0|
|Dream Labs GmbH|0x06C4|
|Dreem SAS|0x0B94|
|Drekker Development Pty. Ltd.<br>Bluetooth SIG Proprietary|0x0343<br>P|



Page 356 of 442 

**Assigned Numbers** / Document 

**2026-06-23** 

|dricos, Inc.|0x096A|
|---|---|
|DSEA A/S|0x0082|
|DualNetworks SA|0x0D77|
|Ducere Technologies Pvt Ltd|0x0312|
|Duke Manufacturing Co|0x0B2F|
|Duracell U.S. Operations Inc.|0x057B|
|Durag GmbH|0x0BD0|
|Duravit AG|0x0ADC|
|Dyaco International Inc.|0x0E51|
|Dyeware, LLC|0x06CB|
|Dymo|0x0B6A|
|Dynamic Controls|0x0175|
|Dynamic Devices Ltd|0x01E5|
|DYNAMOX S/A|0x0D6D|
|Dynaudio A/S|0x0EDF|
|Dynetrex Solutions Inc.|0x0EAC|
|DyOcean|0x049C|
|DYPHI|0x09B2|
|Dyson Technology Limited|0x0A12|
|e-moola.com Pty Ltd|0x07F0|
|E.F. Johnson Company|0x0D98|
|E.G.O. Elektro-Geraetebau GmbH|0x0276|
|E.J. Brooks Company|0x107B|
|e.solutions|0x0115|
|EAGLE DETECTION SA|0x074E|
|EAGLE KINGDOM TECHNOLOGIES LIMITED|0x0AD1|
|EAR TEKNIK ISITME VE ODIOMETRI CIHAZLARI SANAYI VE TICARET<br>ANONIM SIRKETI|0x09D6|
|Earda Technologies Co.,Ltd|0x0AFE|
|Earfun Technology (HK) Limited|0x0D72|
|Eargo, Inc.|0x0329|
|Easee AS|0x0C2E|
|Eastern Partner Limited|0x0E15|
|Easy Measure Co., Ltd.|0x0F24|
|Eaton Corporation|0x035C|
|Eberspaecher Climate Control Systems GmbH|0x0CB4|



Bluetooth SIG Proprietary 

Page 357 of 442 

**Assigned Numbers** / Document 

**2026-06-23** 

|eBest IOT Inc.|0x0437|
|---|---|
|eBet Gaming Sytems Pty Limited|0x0E4F|
|ebm-papst Mulfingen GmbH & Co. KGaA & Co. KG|0x0F51|
|EC sense co., Ltd|0x0C1E|
|ECARX (Hubei) Tech Co.,Ltd.|0x0E2D|
|ECCEL CORPORATION SAS|0x0C9E|
|Eccrine Systems, Inc.|0x0690|
|ECD Electronic Components GmbH Dresden|0x0807|
|ecobee Inc.|0x07D6|
|Ecolab Inc.|0x0BB0|
|Edamic|0x021D|
|Eden Software Consultants Ltd.|0x00E5|
|Edge Semiconductors Inc.|0x0F1C|
|Edifier International Limited|0x07E0|
|EDPS|0x051C|
|Eello LLC|0x0A9B|
|Efento|0x0DAB|
|Eforthink Technology Co., Ltd.|0x0FC8|
|egojin co,.ltd|0x0EB9|
|Ehong Technology Co.,Ltd|0x0DE5|
|Eiritsu Electronics Industry Co., Ltd.|0x0F9A|
|EISST Ltd|0x0298|
|Eko Health, Inc.|0x0E10|
|Ekomini inc.|0x027A|
|ELA Innovation|0x0757|
|ELAD srl|0x01D5|
|Elatec GmbH|0x0752|
|Elecs Industry Co.,Ltd.|0x05B6|
|Electronic Design Lab|0x032A|
|Electronic Sensors, Inc.|0x0B45|
|Electronic Temperature Instruments Ltd|0x0376|
|Electronic Theatre Controls|0x095D|
|ELECTRONICA INTEGRAL DE SONIDO S.A.|0x0636|
|Electronics Tomorrow Limited|0x02C7|
|Electronics4All Inc.|0x0D1D|
|ElectronX design<br>Bluetooth SIG Proprietary|0x0CFC<br>P|



Page 358 of 442 

**Assigned Numbers** / Document 

**2026-06-23** 

|Elekon AG|0x0936|
|---|---|
|Element Products, Inc.|0x070B|
|Elemental Machines, Inc.|0x05E3|
|Eli Lilly and Company|0x0728|
|ELIAS GmbH|0x0294|
|Elimo Engineering Ltd|0x09BA|
|ElitEngineering LLC|0x0F2B|
|ELLEA INGEGNERIA SRL UNIPERSONALE|0x0EEE|
|Ellenby Technologies, Inc.|0x0AB4|
|Ellisys|0x030A|
|ELPRO-BUCHS AG|0x0982|
|Elstat Electronics Ltd.|0x0AB5|
|Eltako GmbH|0x0683|
|EM Microelectronic-Marin SA|0x005A|
|Embecta Corp.|0x0C28|
|Embedded Devices Co. Company|0x089C|
|Embedded Electronic Solutions Ltd. dba e2Solutions|0x0385|
|Embedded Engineering Solutions LLC|0x0C75|
|Embedded Fitness B.V.|0x084F|
|Embedded Solutions LLC|0x0EB7|
|EMBEINT INC|0x0DE4|
|Ember Technologies, Inc.|0x03C1|
|emberlight|0x0169|
|EMBR labs, INC|0x090B|
|Embrava Pty Ltd|0x0A1B|
|Emergency Lighting Products Limited|0x0865|
|Emerja Corporation|0x0C68|
|Emerson Electric Co.|0x04DF|
|Emlid Limited|0x0A7A|
|Emlid Tech Kft.|0x0CBB|
|Emotion Fitness GmbH & Co. KG|0x0B54|
|Empatica Srl|0x02D1|
|EMS Integrators, LLC|0x0B4B|
|ENABLEWEAR LLC|0x0E71|
|Endress+Hauser|0x0245|
|Endur ID, Inc.<br>Bluetooth SIG Proprietary|0x0F12<br>P|



Page 359 of 442 

**Assigned Numbers** / Document 

**2026-06-23** 

|Enequi AB|0x06FA|
|---|---|
|ENERGOUS CORPORATION|0x01A7|
|Energy Technology and Control Limited|0x0D05|
|Eneso Tecnologia de Adaptacion S.L.|0x08E6|
|ENGAGENOW DATA SCIENCES PRIVATE LIMITED|0x09A2|
|Engineered Audio, LLC.|0x04DB|
|Engineered Medical Technologies|0x076B|
|ENLESS WIRELESS|0x0EA0|
|Enlighted Inc|0x0335|
|EnOcean GmbH|0x03DA|
|EnPointe Fencing Pty Ltd|0x0888|
|ENSESO LLC|0x093B|
|Ensto Oy|0x0628|
|Enterlab ApS|0x031D|
|EntWick Co.|0x0BC2|
|EPIC S.R.L.|0x07BB|
|Epic Systems Co., Ltd.|0x08A2|
|EPIFIT|0x0894|
|Epsilon Electronics,lnc|0x0C47|
|eQ-3 AG|0x0F7D|
|EQOM SSC B.V.|0x0CD2|
|Equinux AG|0x0086|
|Eran Financial Services LLC|0x0A25|
|Ergodriven Inc|0x0CFF|
|ERi, Inc|0x010B|
|Ericsson AB|0x0000|
|ERM Electronic Systems LTD|0x04AE|
|ESCEA LIMITED|0x0C52|
|ESEMBER LIMITED LIABILITY COMPANY|0x0783|
|eSenseLab LTD|0x081F|
|Esmé Solutions|0x0BFD|
|ESNAH|0x0DD0|
|Espressif Systems (Shanghai) Co., Ltd.|0x02E5|
|Esrille Inc.|0x03A8|
|ESS Embedded System Solutions Inc.|0x06FD|
|essentim GmbH|0x05DD|



Bluetooth SIG Proprietary 

Page 360 of 442 

**Assigned Numbers** / Document 

**2026-06-23** 

|Essex Electronics|0x0327|
|---|---|
|Essity Hygiene and Health Aktiebolag|0x0707|
|Estimote, Inc.|0x015D|
|ESTOM Infotech Kft.|0x08D0|
|ESYLUX|0x032B|
|ETA SA|0x0231|
|ETABLISSEMENTS GEORGES RENAULT|0x07B7|
|eTactica ehf|0x0999|
|Etekcity Corporation|0x06D0|
|Etesian Technologies LLC|0x0341|
|Ethos Group, Inc.|0x0F92|
|ETO GRUPPE TECHNOLOGIES GmbH|0x0DC8|
|Etymotic Research, Inc.|0x0838|
|Eugster Frismag AG|0x0319|
|Eurotronik Kranj d.o.o.|0x066E|
|Eve Systems GmbH|0x00CE|
|Everykey Inc.|0x0174|
|Evident Corporation|0x0CAE|
|Evluma|0x00C9|
|Evolutive Systems SL|0x0E09|
|EVVA Sicherheitstechnologie GmbH|0x0869|
|Ex Makhina Inc.|0x0D44|
|Exeger Operations AB|0x0C33|
|EXELIO S.R.L.|0x1046|
|EXEO TECH CORPORATION|0x08A1|
|EXFO, Inc.|0x052D|
|Exon Sp. z o.o.|0x03AA|
|exoTIC Systems|0x071D|
|Expai Solutions Private Limited|0x0676|
|Expain AS|0x0375|
|Extron Design Services|0x018D|
|Eyefi, Inc.|0x031E|
|F.I.P. FORMATURA INIEZIONE POLIMERI - S.P.A.|0x0D61|
|F5 Sports, Inc|0x071C|
|Fabtronics Australia Pty Ltd|0x0447|
|FactorySense<br>Bluetooth SIG Proprietary|0x0EB0<br>P|



Page 361 of 442 

**Assigned Numbers** / Document 

**2026-06-23** 

|Fairbanks Scales Inc.|0x105B|
|---|---|
|FairPhone B.V|0x10CA|
|Fanstel Corp|0x0634|
|Fantini Cosmi s.p.a.|0x073B|
|Farm Jenny LLC|0x05D8|
|farmunited GmbH|0x0DC9|
|FarSite Communications Limited|0x0478|
|Fasetto, Inc.|0x0C14|
|Fathom Systems Inc.|0x0463|
|Fatigue Science|0x0417|
|Fauna Audio GmbH|0x0976|
|Favero Electronics Srl|0x0364|
|FAZEPRO LLC|0x0AA4|
|FAZUA GmbH|0x0B06|
|FDK CORPORATION|0x0191|
|FedEx Services|0x0141|
|Feedback Sports LLC|0x0983|
|Felion Technologies Company Limited|0x0DBC|
|Fen Systems Ltd.|0x0DA1|
|Fender Musical Instruments|0x0410|
|FengFan (BeiJing) Technology Co, Ltd|0x0234|
|Ferrari s.p.a.|0x1048|
|FESTINA LOTUS SA|0x0D1E|
|Fetch My Pet|0x01CB|
|ffly4u|0x03E5|
|FIAMM|0x01A1|
|FIBRO GmbH|0x0514|
|Fidure Corp.|0x0C6D|
|FIELD DESIGN INC.|0x0B78|
|Field Line Automation Ltd|0x1063|
|FiftyThree Inc.|0x0247|
|Filo Srl|0x0E45|
|final Inc.|0x0E7C|
|Finch Technologies Ltd.|0x0780|
|Finder S.p.A.|0x0561|
|FINSECUR|0x03D7|



Bluetooth SIG Proprietary 

Page 362 of 442 

**Assigned Numbers** / Document 

**2026-06-23** 

|FIOR & GENTZ|0x06CE|
|---|---|
|First Design System Inc.|0x0AF8|
|First Light Technologies Ltd.|0x0947|
|Fisher & Paykel Healthcare|0x059F|
|Fitnexa Inc|0x0F98|
|Fitz Inc.|0x0DEB|
|Five Interactive, LLC dba Zendo|0x025B|
|FiveCo Sarl|0x070E|
|Fjorden Electra AS|0x0BB2|
|Flaircomm Microelectronics Inc.|0x0B00|
|Fledt & Meiton Marin AB|0x0F53|
|Flender GmbH|0x0BB3|
|FLEURBAEY BVBA|0x026E|
|flexlog GmbH|0x10A1|
|Flexoptix GmbH|0x0AC5|
|Flextronic GmbH|0x0DED|
|Flextronics International USA Inc.|0x0813|
|Fliegl Agrartechnik GmbH|0x02D9|
|FlightSafety International|0x01AD|
|Flint Rehabilitation Devices, LLC|0x02DD|
|FLINTEC UK LIMITED|0x0F32|
|Flipnavi Co.,Ltd.|0x056A|
|Flipper Devices Inc.|0x0E29|
|FLIR Systems AB|0x0AE9|
|Flitsmeister B.V.|0x0FAC|
|FLO SCIENCES, LLC|0x0F1A|
|FLORLINK, INC.|0x0F72|
|Flosonics Medical|0x0A04|
|FlowMotion Technologies AS|0x069F|
|Fluke Corporation|0x025E|
|FMW electronic Futterer u. Maier-Wolf OHG|0x0605|
|Focus fleet and fuel management inc|0x0427|
|Focus Ingenieria SRL|0x0A68|
|Focus Systems Corporation|0x0139|
|FOGO|0x0E21|
|Foil, Inc.<br>Bluetooth SIG Proprietary|0x0A98<br>P|



Page 363 of 442 

**Assigned Numbers** / Document 

**2026-06-23** 

|Follow Sense Europe B.V.|0x0A52|
|---|---|
|foolography GmbH|0x03EF|
|Footmarks|0x0516|
|Force Impact Technologies|0x0769|
|Forciot Oy|0x0620|
|Ford Motor Company|0x0723|
|Form Athletica Inc.|0x067D|
|Form Lifting, LLC|0x01FB|
|Fortiori Design LLC|0x0631|
|FORTRONIK storitve d.o.o.|0x054E|
|Forward Thinking Systems LLC.|0x0CBF|
|Foshan Viomi Electrical Technology Co., Ltd|0x0C21|
|Fosilicon Co., Ltd|0x10B7|
|FOSS Analytical A/S|0x103A|
|Foster Electric Company, Ltd|0x0230|
|Foundation Engineering LLC|0x050F|
|FoundersLane GmbH|0x0B7D|
|Fourth Evolution Inc|0x0603|
|Foxble, LLC|0x0831|
|Fracarro Radioindustrie SRL|0x058C|
|FRAGRANCE DELIVERY TECHNOLOGIES LTD|0x0843|
|Framason Audio S.A|0x1062|
|Franceschi Marina snc|0x04DA|
|Franklin Control Systems|0x0DB4|
|FRANKLIN TECHNOLOGY INC|0x055B|
|Franz Kaldewei GmbH&Co KG|0x0AD8|
|Fraunhofer IIS|0x08A9|
|Frecce LLC|0x07EC|
|FREDERIQUE CONSTANT SA|0x03B9|
|Free2move AB|0x0053|
|Freedman Electronics Pty Ltd|0x0A7D|
|Freedom Innovations|0x01E4|
|FREELAP SA|0x0363|
|Freescale Semiconductor, Inc.|0x01FF|
|Fresh n Rebel B.V.|0x0DD2|
|Freshape SA<br>Bluetooth SIG Proprietary|0x0F13<br>P|



Page 364 of 442 

**Assigned Numbers** / Document 

**2026-06-23** 

|Fresnel Technologies, Inc.|0x0BE7|
|---|---|
|Friday Home Aps|0x09E3|
|Friday Labs Limited|0x041A|
|frogblue TECHNOLOGY GmbH|0x05D2|
|FrontAct Co., Ltd.|0x0E0D|
|Frontiergadget, Inc.|0x0534|
|Frost Solutions, LLC|0x0D3A|
|FUBA Automotive Electronics GmbH|0x06D7|
|Fugoo, Inc.|0x0101|
|FUJI INDUSTRIAL CO.,LTD.|0x0307|
|Fuji Xerox Co., Ltd|0x05ED|
|Fujian Newland Auto-ID Tech. Co., Ltd.|0x0B5D|
|FUJIFILM Corporation|0x04D8|
|FUJIMIC NIIGATA, INC.|0x078E|
|Fujita Electric Works, Ltd|0x0EEB|
|FUJITSU COMPONENT LIMITED|0x0D28|
|Fujitsu Limited|0x02EA|
|Fullpower Technologies, Inc.|0x01EF|
|FulScience Automotive Electronics Co., Ltd.|0x0FC4|
|FUN FACTORY GmbH|0x0B51|
|Funai Electric Co., Ltd.|0x0090|
|Fundacion Tecnalia Research and Innovation|0x0924|
|FUSEAWARE LIMITED|0x0AFF|
|FUTEK ADVANCED SENSOR TECHNOLOGY, INC|0x0AEC|
|FUTURE INTELLIGENCE TECHNOLOGY SINGAPORE PTE. LTD.|0x1085|
|G-Vision GmbH|0x0F4F|
|G-wearables inc.|0x01D6|
|G24 Power Limited|0x0256|
|GA|0x0971|
|Galileo Technology Limited|0x0A3B|
|Gallagher Group|0x03C9|
|Gantner Electronic GmbH|0x0403|
|Garage Smart, Inc.|0x0509|
|Garmin International, Inc.|0x0087|
|Garnet Instruments Ltd.|0x0CC0|
|GASTEC CORPORATION|0x0795|



Bluetooth SIG Proprietary 

Page 365 of 442 

**Assigned Numbers** / Document 

**2026-06-23** 

|Gastron Co.,Ltd|0x10DE|
|---|---|
|GB Solution co.,Ltd|0x088C|
|GCT Semiconductor|0x002D|
|GD Midea Air-Conditioning Equipment Co., Ltd.|0x06A8|
|GE HEALTHCARE TECHNOLOGIES INC.|0x0F02|
|GEAR RADIO ELECTRONICS CORP.|0x09D5|
|GEARBAC TECHNOLOGIES INC.|0x0DC3|
|Geberit International AG|0x0602|
|Gecko Health Innovations, Inc.|0x0181|
|Geeknet, Inc.|0x0B72|
|Geeksme S.L.|0x07E4|
|Gelliner Limited|0x01F7|
|GeLo Inc|0x00C8|
|Gema Switzerland GmbH|0x0670|
|Gemalto|0x026A|
|Gemstone Lights Canada Ltd.|0x0C95|
|GEMU|0x0C1C|
|Genedrive Diagnostics Ltd|0x07BD|
|Geneq Inc.|0x00C2|
|Generac Corporation|0x0DA6|
|General Electric Company|0x01B7|
|General Laser GmbH|0x0DE9|
|General Luminaire (Shanghai) Co., Ltd.|0x0841|
|General Motors|0x0068|
|General Resistance, LLC|0x0FB0|
|Genetus inc.|0x0D51|
|Genevac Ltd|0x02BC|
|Gennum Corporation|0x003B|
|Gentex Corporation|0x0B47|
|Gentle Energy Corp.|0x0B8E|
|Geo Radar AI Private Limited|0x0FAB|
|Geoforce Inc.|0x009D|
|Geopal system A/S|0x0A64|
|GEOPH, LLC|0x0DCB|
|Geophysical Technology Inc.|0x01AA|
|Georg Fischer AG<br>Bluetooth SIG Proprietary|0x0692<br>P|



Page 366 of 442 

**Assigned Numbers** / Document 

**2026-06-23** 

|Geosatis SA|0x052C|
|---|---|
|Geotab|0x0275|
|GERTEC BRASIL LTDA.|0x0342|
|Geva Sol B.V.|0x0C15|
|GGEC America, Inc.|0x0ECF|
|Giatec Scientific Inc.|0x0301|
|Gibson Guitars|0x0062|
|Gibson, Inc.|0x0ED0|
|GigaDevice Semiconductor Inc.|0x0C2B|
|GIGALANE.CO.,LTD|0x01A2|
|Gigaset Technologies GmbH|0x0180|
|Gill Electronics|0x01D2|
|GILL INSTRUMENTS LIMITED|0x0F38|
|GILSON SAS|0x0C6B|
|Gilvader|0x02DA|
|Gimbal Inc.|0x008C|
|Gimer medical|0x0895|
|GimmiSys GmbH|0x094C|
|GiP Innovation Tools GmbH|0x0637|
|GiPStech S.r.l.|0x0A93|
|Gira Giersiepen GmbH & Co. KG|0x0584|
|GISTIC|0x0336|
|GL Solutions K.K.|0x0962|
|Glacial Ridge Technologies|0x0262|
|Glamo Inc.|0x0C22|
|Glass Security Pte Ltd|0x0738|
|Glenview Software Corporation|0x077F|
|Global Electronics|0x106C|
|Global Link Distribution Corp.|0x0F27|
|Global Satellite Engineering|0x0DD3|
|GlobalMed|0x0AE3|
|Globalstar, Inc.|0x0576|
|Globalworx GmbH|0x06A2|
|Globe (Jiangsu) Co., Ltd|0x0945|
|GLOWFORGE INC.|0x0BA8|
|GLP German Light Products GmbH<br>Bluetooth SIG Proprietary|0x0AEF<br>P|



Page 367 of 442 

**Assigned Numbers** / Document 

**2026-06-23** 

|Glutz AG|0x0E92|
|---|---|
|GN Hearing|0x0067|
|GN Hearing A/S|0x0089|
|Goerdyna Group Co., Ltd|0x0ED4|
|GoerTek Dynaudio Co., Ltd.|0x05AC|
|GOKI PTY LTD|0x0E62|
|Goodnet, Ltd|0x021E|
|Gooee Limited|0x04D5|
|Google|0x00E0|
|Google LLC|0x018E|
|Gooligum Technologies Pty Ltd|0x0926|
|GOOOLED S.R.L.|0x04CE|
|Gopod Group Holding Limited|0x0E8C|
|GoPro, Inc.|0x02F2|
|Gordon Murray Design Limited|0x0C11|
|GORDON MURRAY GROUP LIMITED|0x106B|
|Gosuncn Technology Group Co., Ltd.|0x0D43|
|Gozio Inc.|0x01FA|
|GP Acoustics International Limited|0x0EBC|
|GPSI Group Pty Ltd|0x0128|
|Graesslin GmbH|0x076D|
|Grand Centrix GmbH|0x0448|
|Grandex International Corporation|0x0BAB|
|Granite River Solutions, Inc.|0x07AB|
|Grape Systems Inc.|0x0142|
|Gravaa B.V.|0x0D80|
|Gravity (Shenzhen)Space Technology Co., Ltd|0x1058|
|Grayhill Inc.|0x108A|
|GREE Electric Appliances, Inc. of Zhuhai|0x0D23|
|Green Throttle Games|0x00AC|
|Greennote Inc,|0x0A84|
|greenTEG AG|0x0BF6|
|Greenwald Industries|0x0685|
|Grob Technologies, LLC|0x03DF|
|Grote Industries|0x05C2|
|Group Lotus Limited<br>Bluetooth SIG Proprietary|0x0B9F<br>P|



Page 368 of 442 

**Assigned Numbers** / Document 

**2026-06-23** 

|Group Sense Ltd.|0x0073|
|---|---|
|Grundfos A/S|0x0328|
|GS TAG|0x05F5|
|GT-tronics HK Ltd|0x04A3|
|Guangdong Hengqin Xingtong Technology Co.,ltd.|0x0E81|
|Guangdong Nanguang Photo&Video Systems Co., Ltd.|0x0E76|
|GuangDong Oppo Mobile Telecommunications Corp., Ltd.|0x079A|
|Guangzhou FiiO Electronics Technology Co.,Ltd|0x04A5|
|Guangzhou Honor Microelectronic Co.,Ltd.|0x0ECE|
|GuangZhou KuGou Computer Technology Co.Ltd|0x0748|
|Guangzhou SuperSound Information Technology Co.,Ltd|0x0724|
|Guard RFID Solutions Inc.|0x0B16|
|Guardtec, Inc.|0x04E0|
|Guide ID B.V.|0x096B|
|Guilin Zhishen Information Technology Co.,Ltd.|0x085D|
|Guillemot Corporation|0x02C2|
|Gunakar Private Limited|0x0696|
|Gunnebo Aktiebolag|0x0D5A|
|Gunwerks, LLC|0x096D|
|GV Concepts Inc.|0x07B0|
|GWA Hygiene GmbH|0x072C|
|Gycom Svenska AB|0x05EC|
|Gymstory B.V.|0x0B98|
|H G M Automotive Electronics, Inc.|0x0B55|
|H+B Hightech GmbH|0x0C96|
|HabitAware, LLC|0x027E|
|Hach - Danaher|0x0693|
|Hager|0x0506|
|HagerEnergy GmbH|0x0D93|
|Hagleitner Hygiene International GmbH|0x05F9|
|HAINBUCH GMBH SPANNENDE TECHNIK|0x0A8A|
|Hala Systems, Inc.|0x0B32|
|Hamilton Professional Services of Canada Incorporated|0x0750|
|HANA Micron|0x01B9|
|Hangzhou BroadLink Technology Co., Ltd.|0x0B93|
|Hangzhou Hikvision Digital Technology Co., Ltd.<br>Bluetooth SIG Proprietary|0x0E25<br>P|



Page 369 of 442 

**Assigned Numbers** / Document 

**2026-06-23** 

|Hangzhou iMagic Technology Co., Ltd|0x0524|
|---|---|
|Hangzhou Microimage Software Co.,Ltd.|0x0E18|
|Hangzhou Nano IC Technologies Co,. Ltd|0x0FB5|
|Hangzhou NationalChip Science & Technology Co.,Ltd|0x0DC4|
|Hangzhou Sciener Smart Technology Co., Ltd.|0x0F85|
|Hangzhou SDIC Microelectronics Inc.|0x10AF|
|Hangzhou Sneuro Medical Co., Ltd.|0x1050|
|Hangzhou Tuya Information Technology Co., Ltd|0x07D0|
|Hangzhou Yaguan Technology Co. LTD|0x09F9|
|Hangzhou Zhaotong Microelectronics Co., Ltd.|0x0DF7|
|Hanlynn Technologies|0x007B|
|Hanna Instruments, Inc.|0x078F|
|Hans Dinslage GmbH|0x0613|
|HANSHIN ELECTRIC RAILWAY CO.,LTD.|0x02B5|
|Hanshow Technology Co.,Ltd.|0x0F9B|
|Hapn Holdings, LLC|0x0FB7|
|HAPPIEST BABY, INC.|0x0A69|
|HAPPLABS SOFTWARE PRIVATE LIMITED|0x0F01|
|Happy Health, Inc.|0x0C04|
|happybrush GmbH|0x06F9|
|Haptech, Inc.|0x0DFE|
|HARADA INDUSTRY CO., LTD.|0x0B73|
|Harbortronics, Inc.|0x04AB|
|Hardcoder Oy|0x0B7B|
|Harley-Davidson Motor Company|0x10BC|
|HARMAN CO.,LTD.|0x075A|
|Harman International Industries, Inc.|0x0057|
|HASWARE Inc.|0x01C6|
|Hatch Baby, Inc.|0x0434|
|Havells India Limited|0x0B40|
|Hawkeye Surgical Lighting LLC|0x10DF|
|HAYWARD INDUSTRIES, INC.|0x0CEA|
|hDrop Technologies Inc.|0x0DF3|
|Health Data Insight C.I.C.|0x0F40|
|Healthcare Technology Limited|0x0EFF|
|Healthwear Technologies (Changzhou)Ltd<br>Bluetooth SIG Proprietary|0x0326<br>P|



Page 370 of 442 

**Assigned Numbers** / Document 

**2026-06-23** 

|Hearing Lab Technology|0x0585|
|---|---|
|Heartland Payment Systems|0x034F|
|hearX Group (Pty) Ltd|0x0BA7|
|Heath Consultants Inc.|0x0C42|
|Heavys Inc|0x0C8B|
|Hehku Company Oy|0x10D2|
|Heilongjiang Tianyouwei Electronics Co.,Ltd.|0x0E14|
|Heinrich Kopp GmbH|0x0E08|
|Hekatron Vertriebs GmbH|0x055E|
|Helge Kaiser GmbH|0x0D6A|
|Helios Sports, Inc.|0x0855|
|HELLA GmbH & Co. KGaA|0x0DBE|
|Hello Inc.|0x03EA|
|Helvar Ltd|0x0438|
|HENDON SEMICONDUCTORS PTY LTD|0x058F|
|Hendrickson USA , L.L.C|0x0D66|
|Henway Technologies, LTD.|0x0938|
|Herbert Waldmann GmbH & Co. KG|0x04A4|
|HerdDogg, Inc|0x0959|
|Hero Workout GmbH|0x0940|
|Herschel Infrared Ltd|0x0EC3|
|HERUTU ELECTRONICS CORPORATION|0x0CAB|
|Hestan Smart Cooking Inc.|0x03F1|
|Hewlett Packard Enterprise|0x011B|
|Hexagon Aura Reality AG|0x0DAF|
|HEXAGON METROLOGY DIVISION ROMER|0x0829|
|Hexology|0x0AE6|
|HHO (Hangzhou) Digital Technology Co., Ltd.|0x0CD6|
|Hibino Corporation|0x0F9E|
|HID Global|0x0124|
|High Entropy, LLC|0x0EC2|
|HIGH HIT ENTERPRISE CO., LTD.|0x1065|
|Hill-Rom|0x08FC|
|Hilti AG|0x030C|
|HIMSA II K/S|0x0BBF|
|HiSilicon Technologies CO., LIMITED<br>Bluetooth SIG Proprietary|0x010F<br>P|



Page 371 of 442 

**Assigned Numbers** / Document 

**2026-06-23** 

|Hitachi Industrial Equipment Systems Co.,Ltd.|0x0F0C|
|---|---|
|Hitachi Ltd|0x0029|
|HITO INC|0x0E22|
|HitSeed Oy|0x06BE|
|Hive Soundz inc.|0x0EAA|
|Hive-Zox International SA|0x0E19|
|HiViz Lighting, Inc.|0x0C54|
|HLI Solutions Inc.|0x06DF|
|HLP Controls Pty Limited|0x0786|
|HM Electronics, Inc.|0x034C|
|HMD Global Oy|0x0CC3|
|HMS Industrial Networks AB|0x06AB|
|Hoffmann SE|0x08A3|
|Holman Industries|0x0374|
|HoloKit, Inc.|0x0798|
|Honda Motor Co., Ltd.|0x0915|
|Hondata, Inc.|0x0FD7|
|Honeywell International Inc.|0x0526|
|Hong Kong Bouffalo Lab Limited|0x07AF|
|HONG KONG COMMUNICATIONS COMPANY LIMITED|0x0E78|
|Hong Kong Future lntelligent Technology Co., Limited|0x1073|
|HONGKONG NANO IC TECHNOLOGIES CO., LIMITED|0x0525|
|Hongkong OnMicro Electronics Limited|0x01BF|
|Honor Device Co., Ltd.|0x09C6|
|Hooked Society Oy Ltd|0x0FB3|
|hoots classic GmbH|0x07D5|
|HORIBA, Ltd.|0x0C40|
|Hormann KG Antriebstechnik|0x07B4|
|HOSEOTELNET Co., Ltd.|0x10B3|
|Hosiden Besson Limited|0x0E47|
|Hosiden Corporation|0x00DD|
|Hotron Co., Ltd.|0x1068|
|Houston Radar LLC|0x06EB|
|HOUWA SYSTEM DESIGN, k.k.|0x01CE|
|HP Tuners|0x0DA4|
|HP, Inc.<br>Bluetooth SIG Proprietary|0x0065<br>P|



Page 372 of 442 

**Assigned Numbers** / Document 

**2026-06-23** 

|HQ Inc|0x031B|
|---|---|
|HTC Corporation|0x02ED|
|HUAWEI Technologies Co., Ltd.|0x027D|
|Hubei Yuan Times Technology Co., Ltd.|0x0BD1|
|Huf Hülsbeck & Fürst GmbH & Co. KG|0x070A|
|Hug Technology Ltd|0x066F|
|Hugo Muller GmbH & Co KG|0x0632|
|HuiTong intelligence Company Limited|0x0E64|
|HUIZHOU DESAY SV AUTOMOTIVE CO., LTD.|0x014D|
|Huizhou Foryou General Electronics Co., Ltd.|0x0E2A|
|Huizhou Meicanxin Electronics Technology Co.,Ltd|0x0F46|
|Human, Incorporated|0x05E1|
|HungYi Microelectronics Co.,Ltd.|0x09C5|
|Hunter Douglas Inc|0x0819|
|Hunter Industries Incorporated|0x0D95|
|Huso, INC|0x0BBA|
|Husqvarna AB|0x0426|
|HWM-Water Limited|0x0DBF|
|Hx Engineering, LLC|0x09CB|
|Hydro Electronic Devices, Inc.|0x0F80|
|Hydro-Gear Limited Partnership|0x0887|
|Hyena Inc.|0x0E6A|
|Hygiene IQ, LLC.|0x0B22|
|Hyginex, Inc.|0x02B4|
|Hykso Inc.|0x0C58|
|HyolimXE Co., Ltd.|0x0F61|
|HYPER ICE, INC.|0x08BA|
|Hypershell Co., Ltd|0x0FC5|
|Hyundai Motor Company|0x0826|
|HYUPSUNG MACHINERY ELECTRIC CO., LTD.|0x0D59|
|Häfele GmbH & Co KG|0x07E9|
|i+D3 S.L.|0x01B8|
|i-developer IT Beratung UG|0x0400|
|i-focus Co.,Ltd|0x0D76|
|i-SENS, inc.|0x089E|
|I-SYST inc.|0x0177|



Bluetooth SIG Proprietary 

Page 373 of 442 

**Assigned Numbers** / Document 

**2026-06-23** 

|I.FARM, INC.|0x067B|
|---|---|
|i.Tech Dynamic Global Distribution Ltd.|0x0099|
|IACA electronique|0x0303|
|iaconicDesign Inc.|0x07C7|
|IAI Corporation|0x0521|
|iApartment co., ltd.|0x0474|
|IBA Dosimetry GmbH|0x0970|
|IBM Corp.|0x0003|
|iCOGNIZE GmbH|0x06B7|
|Icom inc.|0x0475|
|Icon Health and Fitness|0x01A5|
|iconmobile GmbH|0x05E9|
|ICP Systems B.V.|0x0B4E|
|ICU tech GmbH|0x0B7F|
|IDEATRONIK Limited Liability Company|0x0F0E|
|IDEC|0x0BD2|
|Identita Inc.|0x0E91|
|Identiv, Inc.|0x0263|
|iDesign s.r.l.|0x0470|
|IDIBAIX enginneering|0x0718|
|IDX Company, Ltd.|0x0F94|
|ifLink Open Community|0x09AF|
|ifly|0x08CD|
|iFLYTEK (Suzhou) Technology Co., Ltd.|0x0DF1|
|ifm electronic gmbh|0x0D58|
|Igarashi Engineering|0x0489|
|igloohome|0x05BA|
|Iguanavation, Inc.|0x0733|
|IK Multimedia Production srl|0x0214|
|ikeGPS|0x0210|
|iKeyless, LLC|0x0F18|
|ILLUMAGEAR, Inc.|0x0B2C|
|Illusory Studios LLC|0x074A|
|Illuxtron international B.V.|0x02A7|
|iLOQ Oy|0x064D|
|iLumi Solutions Inc.|0x03BF|



Bluetooth SIG Proprietary 

Page 374 of 442 

**Assigned Numbers** / Document 

**2026-06-23** 

|imagiLabs AB|0x0880|
|---|---|
|IMAGINATION TECHNOLOGIES LTD|0x02F9|
|Imagine Marketing Limited|0x0CD1|
|IMI Hydronic Engineering International SA|0x08ED|
|iMicroMed Incorporated|0x042B|
|Imostar Technologies Inc.|0x0C50|
|Impact Biosystems, Inc.|0x0B3A|
|IMPACT-BZ LTD|0x107D|
|Impossible Camera GmbH|0x02A0|
|Impulse Wellness LLC|0x0D9E|
|IN Phase International lTD|0x0F81|
|In2things Automation Pvt. Ltd.|0x03CA|
|INCITAT ENVIRONNEMENT|0x0B43|
|inContAlert GmbH|0x10A0|
|Incotex Co. Ltd.|0x0A3A|
|INCUS PERFORMANCE LTD.|0x082D|
|Indagem Tech LLC|0x02F5|
|Indigo Diabetes|0x0A58|
|Indistinguishable From Magic, Inc.|0x0D75|
|indoormap|0x032E|
|InDreamer Techsol Private Limited|0x04F2|
|Industrial Computing Limited|0x108E|
|Industrial Network Controls, LLC|0x06DC|
|INEO ENERGY& SYSTEMS|0x05E5|
|INEO-SENSE|0x0914|
|INEPRO Metering B.V.|0x0E56|
|Inexess Technology Simma KG|0x0299|
|Infineon Technologies AG|0x0009|
|Infinitegra, Inc.|0x0BAA|
|iNFORM Technology GmbH|0x081E|
|INFOTECH s.r.o.|0x0390|
|infyni|0x103F|
|Ingchips Technology Co., Ltd.|0x06AC|
|Ingenieur-Systemgruppe Zahn GmbH|0x00AB|
|Ingenieurbuero Birnfeld UG (haftungsbeschraenkt)|0x0892|
|INGICS TECHNOLOGY CO., LTD.|0x082C|



Bluetooth SIG Proprietary 

Page 375 of 442 

**Assigned Numbers** / Document 

**2026-06-23** 

|Ingy B.V.|0x080C|
|---|---|
|INIA|0x05AD|
|Inito Health Inc.|0x10CE|
|Inmite s.r.o.|0x0158|
|inMusic Brands, Inc|0x00E3|
|Innblue Consulting|0x01DB|
|INNER RANGE PTY. LTD.|0x0AB3|
|Innocent Technology Co., Ltd.|0x0CC9|
|InnoCon Medical ApS|0x0770|
|Innogando S.L.|0x0F8A|
|Innohome Oy|0x0A73|
|Innokind, Inc.|0x091D|
|Innolux Corporation|0x0B63|
|Innophase Incorporated|0x0873|
|Innoseis|0x05FD|
|Innova Ideas Limited|0x0673|
|INNOVA S.R.L.|0x0C51|
|Innovacionnye Resheniya|0x0BF8|
|INNOVAG PTY. LTD.|0x0BB8|
|InnovaSea Systems Inc.|0x06AD|
|Innovation First, Inc.|0x0677|
|Innovative Design Labs Inc.|0x0A5C|
|Innovative Yachtter Solutions|0x0106|
|InnoVision Medical Technologies, LLC|0x0E43|
|Innoware Development AB|0x09DD|
|Inor Process AB|0x0595|
|INOVA Geophysical, Inc.|0x0821|
|Inovonics Corp|0x08D7|
|INPEAK S.C.|0x0A1C|
|InPlay, Inc.|0x0505|
|inQs Co., Ltd.|0x0686|
|INSIGMA INC.|0x046A|
|Insta GmbH|0x04C6|
|Instabeat, Inc|0x03DB|
|instagrid GmbH|0x08F8|
|Instamic, Inc.|0x0A2F|



Bluetooth SIG Proprietary 

Page 376 of 442 

**Assigned Numbers** / Document 

**2026-06-23** 

|Instinct Performance|0x057D|
|---|---|
|Insulet Corporation|0x0360|
|Integral Memroy Plc|0x0575|
|Integrated Silicon Solution Taiwan, Inc.|0x0041|
|Integrated System Solution Corp.|0x0039|
|Intel Corp.|0x0002|
|Intelletto Technologies Inc.|0x0190|
|IntelliDesign Pty Ltd|0x10D5|
|Intelligenceworks Inc.|0x08A6|
|Intellithings Ltd.|0x06DD|
|Intemo Technologies|0x02F6|
|INTER ACTION Corporation|0x0615|
|Interactio|0x04F9|
|Intermotive,Inc.|0x0832|
|International Forte Group LLC|0x0465|
|Interplan Co., Ltd|0x0212|
|Intricon|0x03C6|
|Intuity Medical|0x0A7F|
|Inugo Systems Limited|0x0A55|
|Inuheat Group AB|0x04B4|
|Inventas AS|0x0ABD|
|Inventel|0x001E|
|Inventronics GmbH|0x0DA9|
|Invisalert Solutions, Inc.|0x0DB0|
|InvisionHeart Inc.|0x0209|
|InVue Security Products Inc|0x0AB1|
|iodyne, LLC|0x0E06|
|IONA Tech LLC|0x0BB7|
|IONIQ Skincare GmbH & Co. KG|0x08B3|
|IORA Technology Development Ltd. Sti.|0x0B02|
|IoSA|0x0C12|
|IoT Instruments Oy|0x03E6|
|IOT Invent GmbH|0x090D|
|IOT Pot India Private Limited|0x03D2|
|IoT Solutions Malta Limited|0x0EF8|
|iota Biosciences, Inc.<br>Bluetooth SIG Proprietary|0x0D73<br>P|



Page 377 of 442 

**Assigned Numbers** / Document 

**2026-06-23** 

|Iotera Inc|0x0244|
|---|---|
|IOTOOLS|0x0B13|
|IOTTIVE (OPC) PRIVATE LIMITED|0x04DC|
|IPextreme, Inc.|0x003D|
|IQAir AG|0x060A|
|IQNEXXT Solutions GmbH|0x0E80|
|Irdeto|0x0AF7|
|IRES Infrarot Energie Systeme GmbH|0x0E39|
|iRhythm Technologies, Inc.|0x0B23|
|iRiding(Xiamen)Technology Co.,Ltd.|0x0352|
|IRIS OHYAMA CO.,LTD.|0x08DF|
|iRobot Corporation|0x0600|
|IRTrans GmbH|0x0F9D|
|ise Individuelle Software und Elektronik GmbH|0x0ACB|
|ISEKI FRANCE S.A.S|0x0D54|
|ISEMAR S.R.L.|0x097F|
|ISEO Serrature S.p.a.|0x0B3E|
|iSi Wearable Safety GmbH|0x0F6E|
|ISSPRO, INC|0x0FC2|
|ista International GmbH|0x0A9F|
|iSwip|0x07B8|
|ITALTRACTOR ITM S.P.A.|0x0DAC|
|ITEC corporation|0x0280|
|Iton Technology Corp.|0x03F6|
|Itron, Inc.|0x01F2|
|Itude|0x02A3|
|ITZ Innovations- und Technologiezentrum GmbH|0x0652|
|IVT Wireless Limited|0x03A0|
|IYO INC.|0x0F49|
|IZITHERM|0x0592|
|IZYBAT|0x1071|
|J Neades Ltd|0x06ED|
|J&M Corporation|0x0052|
|J-J.A.D.E. Enterprise LLC|0x089D|
|J. Wagner GmbH|0x08C5|
|Jaguar Land Rover Limited<br>Bluetooth SIG Proprietary|0x020B<br>P|



Page 378 of 442 

**Assigned Numbers** / Document 

**2026-06-23** 

|James Walker RotaBolt Limited|0x0930|
|---|---|
|Jana Care Inc.|0x040B|
|Jano Life Inc.|0x0EFC|
|janova GmbH|0x0CB3|
|Japan Display Inc.|0x0D24|
|JAPAN TOBACCO INC.|0x09C3|
|Jawbone|0x008A|
|JBX Designs Inc.|0x0704|
|JCM TECHNOLOGIES S.A.|0x0A8D|
|JCT Healthcare Pty Ltd|0x06D6|
|JDRF Electromag Engineering Inc|0x0B70|
|JE electronic a/s|0x0E2B|
|JEPICO Corporation|0x093F|
|Jetro AS|0x028A|
|Jiangsu Qinheng Co., Ltd.|0x0739|
|Jiangsu Teranovo Tech Co., Ltd.|0x068B|
|Jiangsu Toppower Automotive Electronics Co., Ltd.|0x009B|
|Jiangsu XinTongda Electric Technology Co.,Ltd.|0x0E7D|
|Jiangxi Innotech Technology Co., Ltd|0x0A20|
|Jigowatts Inc.|0x03EC|
|JIN CO, Ltd|0x0239|
|JL WORLD CORPORATION LIMITED|0x0F4C|
|JLD Technology Solutions, LLC|0x09DE|
|JLG Industries, Inc.|0x0753|
|JMR embedded systems GmbH|0x072A|
|John Deere|0x0606|
|Johnson Controls, Inc.|0x00B9|
|Johnson Health Tech NA|0x07C4|
|Johnson Outdoors Inc|0x0278|
|Jolla Ltd|0x01E1|
|Jolly Logic, LLC|0x00ED|
|Joovv, Inc.|0x08F0|
|Joule IQ, INC.|0x07ED|
|JRC Mobility Inc.|0x0A48|
|JRM Group Limited|0x068F|
|JSB TECH PTE LTD|0x0923|



Bluetooth SIG Proprietary 

Page 379 of 442 

**Assigned Numbers** / Document 

**2026-06-23** 

|JSK CO., LTD.|0x07FD|
|---|---|
|JT INNOVATIONS LIMITED|0x08CB|
|JUJU JOINTS CANADA CORP.|0x0848|
|Julbo|0x0954|
|Julius Blum GmbH|0x09CE|
|Jumo GmbH & Co. KG|0x0A72|
|June Life, Inc.|0x07C5|
|Jungheinrich Aktiengesellschaft|0x058D|
|JURA Elektroapparate AG|0x0C25|
|JUREN CO., LTD.|0x0F8D|
|JVC KENWOOD Corporation|0x0D10|
|Kabushikigaisha HANERON|0x09E7|
|KAGA FEI Co., Ltd.|0x0DDB|
|KAHA PTE. LTD.|0x0921|
|Kano Computing Limited|0x07D4|
|Kapsch TrafficCom AB|0x0388|
|KARLUNA MUHENDISLIK SANAYI VE TICARET ANONIM SIRKETI|0x0E48|
|Kartographers Technologies Pvt. Ltd.|0x04B1|
|Katerra Inc.|0x075E|
|Kathrein Solutions GmbH|0x0DEA|
|Kayamatics Limited|0x08AD|
|KC Technology Inc.|0x0016|
|KCCS Mobile Engineering Co., Ltd.|0x0ABC|
|KEAN ELECTRONICS PTY LTD|0x09EB|
|KEBA Energy Automation GmbH|0x0C60|
|KEBA Handover Automation GmbH|0x0A7E|
|KEEPEN|0x0D01|
|Keepin Co., Ltd.|0x0AC8|
|Kehwin Technologies Co. Ltd.|0x0F62|
|Keiser Corporation|0x0102|
|KELLER Druckmesstechnik AG|0x0F2A|
|Kemppi Oy|0x0203|
|Kensington Computer Products Group|0x00A0|
|Kent Displays Inc.|0x00F3|
|Kenzen, Inc.|0x0AC3|
|Kerlink|0x0956|



Bluetooth SIG Proprietary 

Page 380 of 442 

**Assigned Numbers** / Document 

**2026-06-23** 

|Kesseböhmer Ergonomietechnik GmbH|0x0C92|
|---|---|
|Ketronixs Sdn Bhd|0x08FE|
|Keycafe Inc.|0x0F73|
|KEYes|0x08D5|
|Keynes Controls Ltd|0x052A|
|KeySafe-Cloud|0x0608|
|KEYTEC,Inc.|0x0C23|
|KHN Solutions LLC|0x09A8|
|KIBLE|0x109C|
|Kickmaker|0x0827|
|KIDO SPORTS CO., LTD.|0x0BF0|
|KidzTek LLC|0x07EE|
|Kiiroo BV|0x05A5|
|Kimberly-Clark|0x03FC|
|Kindeva Drug Delivery L.P.|0x094B|
|Kindhome|0x0D70|
|KINDOO LLP|0x0C31|
|King I Electronics.Co.,Ltd|0x06C3|
|Kinsa, Inc|0x0123|
|Kiteras Inc.|0x0D71|
|KiteSpring Inc.|0x0934|
|KIWI.KI GmbH|0x0DA2|
|KKM COMPANY LIMITED|0x0A53|
|Klipsch Group, Inc.|0x07FA|
|KloudNation|0x01F0|
|Knick Elektronische Messgeraete GmbH & Co. KG|0x0548|
|Knit Sound Company|0x1080|
|KNOG PTY. LTD.|0x0DE0|
|Knox Associates, Inc.|0x1097|
|KOAMTAC INC.|0x0778|
|KOBATA GAUGE MFG. CO., LTD.|0x0ACE|
|Kobian Canada Inc.|0x0445|
|Kocomojo, LLC|0x0173|
|Kodimo Technologies Company Limited|0x08F4|
|Kodira GmbH|0x0EDA|
|Kohler Company<br>Bluetooth SIG Proprietary|0x0594<br>P|



Page 381 of 442 

**Assigned Numbers** / Document 

**2026-06-23** 

|Kohler Mira Limited|0x038A|
|---|---|
|Kohler Ventures, Inc.|0x0F88|
|Koizumi Lighting Technology corp.|0x0BE0|
|Koki Holdings Co., Ltd.|0x0695|
|kokoromil Inc.|0x0E97|
|Kolibree|0x0460|
|Komatsu Ltd.|0x0B9C|
|Komfort IQ, Inc.|0x07E7|
|KOMPAN A/S|0x0790|
|Konami Sports Life Co., Ltd.|0x05FA|
|Konftel AB|0x087D|
|Konica Minolta, Inc.|0x018B|
|Koninklijke Philips N.V.|0x01DD|
|Konova|0x0D7F|
|Kontakt Micro-Location Sp. z o.o.|0x01FD|
|Kopi|0x0672|
|Kopin Corporation|0x041F|
|KOQOON GmbH & Co.KG|0x0DD4|
|Kord Defence Pty Ltd|0x0CCC|
|Kosi Limited|0x04F0|
|KOUKAAM a.s.|0x00FB|
|Koya Medical, Inc.|0x099F|
|KOZO KEIKAKU ENGINEERING Inc.|0x08E1|
|Krog Systems LLC|0x085E|
|KROHNE Messtechnik GmbH|0x0555|
|Kromek Group Plc|0x063B|
|Kronos Incorporated|0x0383|
|KRUXWorks Technologies Private Limited|0x064E|
|KS Technologies|0x00E7|
|KT Micro, Inc.|0x0F84|
|KTS GmbH|0x04D1|
|KUBU SMART LIMITED|0x0DCF|
|KUMHO ELECTRICS, INC|0x09A4|
|Kumho Tire|0x10CD|
|Kupson spol. s r.o.|0x029E|
|KUUKANJYOKIN Co.,Ltd.|0x0A6D|



Bluetooth SIG Proprietary 

Page 382 of 442 

**Assigned Numbers** / Document 

**2026-06-23** 

|Kynesim Ltd|0x0468|
|---|---|
|KYS|0x035D|
|L.S. Research, Inc.|0x00E4|
|L.T.H. Electronics Limited|0x0E5D|
|LAAS ApS|0x0D90|
|Lab Sensor Solutions|0x031C|
|Laerdal Medical AS|0x01CA|
|Laird Connectivity LLC|0x0077|
|Lakecrest Pty Ltd|0x0F86|
|LAMPLIGHT Co., Ltd.|0x05D4|
|Landig + Lava GmbH & Co. KG|0x0F3F|
|Lantern Innovations Incorporated|0x0F50|
|LAONZ Co.,Ltd|0x0866|
|LAPIS Semiconductor Co.,Ltd|0x0179|
|LAST LOCK INC.|0x0E63|
|LAUMAS Elettronica S.r.l.|0x104C|
|Lautsprecher Teufel GmbH|0x0AD2|
|LAVAZZA S.p.A.|0x0393|
|Laxmi Therapeutic Devices, Inc.|0x0D30|
|Lazlo326, LLC.|0x0599|
|LDL TECHNOLOGY|0x063F|
|Le Touch (Shenzhen) Electronics Co., Ltd.|0x0E75|
|Leaftronix Analogic Solutions Private Limited|0x06BB|
|Leapcraft ApS|0x0F5D|
|LECO Corporation|0x0810|
|LED Smart Inc.|0x0C06|
|Ledlenser GmbH & Co. KG|0x0654|
|LEDVANCE GmbH|0x052E|
|Legato Audio, Inc.|0x0FCA|
|Leggett & Platt, Incorporated|0x092D|
|LEGIC Identsystems AG|0x0A5B|
|LEGO System A/S|0x0397|
|LEGRAND|0x0586|
|Leica Camera AG|0x051A|
|Leica Geosystems AG|0x0D09|
|Lely<br>Bluetooth SIG Proprietary|0x055C<br>P|



Page 383 of 442 

**Assigned Numbers** / Document 

**2026-06-23** 

|Lenovo (Singapore) Pte Ltd.|0x02C5|
|---|---|
|lesswire AG|0x0079|
|Leupold & Stevens, Inc.|0x0AF0|
|LEVEL, s.r.o.|0x0AE8|
|Levita|0x0DC6|
|Leviton Mfg. Co., Inc.|0x05BC|
|Lexmark International Inc.|0x025D|
|Lezyne USA Inc.|0x0F08|
|LG Electronics|0x00C4|
|Libratone A/S|0x034B|
|Lichens Innovation inc.|0x0ED3|
|Lichtvision Engineering GmbH|0x09FE|
|Liebherr-Hausgeraete GmbH|0x10D9|
|Lierda Science & Technology Group Co., Ltd.|0x02FE|
|Life Laboratory Inc.|0x0306|
|LifeBEAM Technologies|0x0227|
|LifeScan Inc|0x016D|
|LifeStyle Lock, LLC|0x044C|
|Lighting Science Group Corp.|0x0573|
|Lightnovo ApS|0x10A6|
|LIHJOEN SPEED METER CO., LTD.|0x0E26|
|lilbit ODM AS|0x0B71|
|LIMBOID LLC|0x0D1C|
|Limited Liability Company ”Mikrotikls”|0x094F|
|limited liability company ”Red”|0x0C08|
|LIMNO Co. Ltd.|0x06A4|
|LINAK A/S|0x00A4|
|LINCOGN TECHNOLOGY CO. LIMITED|0x0AA1|
|Lindab AB|0x05D1|
|Linde GmbH|0x10CC|
|Lindinvent AB|0x08C2|
|LINE Corporation|0x02C1|
|Linear Circuits|0x07DD|
|Link Labs, Inc.|0x0A1A|
|LINKEDCHIP TECHNOLOGY INC|0x0F42|
|LinkedSemi Microelectronics (Xiamen) Co., Ltd<br>Bluetooth SIG Proprietary|0x093A<br>P|



Page 384 of 442 

**Assigned Numbers** / Document 

**2026-06-23** 

|LINKIO SAS|0x04AA|
|---|---|
|LINKSYS USA, INC.|0x0BEE|
|Linkura AB|0x0961|
|Linough Inc.|0x054A|
|Lintech GmbH|0x0144|
|LION GROUP, INC.|0x0F0A|
|LIOTYS|0x0FD6|
|Lippert Components, INC|0x05C7|
|Lismore Instruments Limited|0x095B|
|Listen Technologies Corporation|0x09FA|
|Liteboxer Technologies Inc.|0x08C8|
|Littelfuse|0x03FE|
|Littlebird Connected Care, Inc.|0x0FC0|
|littleBits|0x0369|
|Livanova USA, Inc.|0x0669|
|LIVNEX Co.,Ltd.|0x07D3|
|LIXIL Corporation|0x0A0F|
|LL Tec Group LLC|0x0BCF|
|LLC ”MEGA-F service”|0x0380|
|LLC Navitek|0x0737|
|LM Technologies Ltd|0x01B6|
|Lockn Technologies Private Limited|0x0A4D|
|Lockswitch Sdn Bhd|0x0C5A|
|Locoroll, Inc|0x0503|
|Locus Positioning|0x0345|
|Locus Robotics Corp.|0x0FBD|
|Lodestar Technology Inc.|0x0F47|
|Loewe Technology GmbH|0x0E59|
|LOGICDATA Electronic & Software Entwicklungs GmbH|0x0547|
|LogiLube, LLC|0x095C|
|LogiLube, LLC|0x0953|
|Logitech International SA|0x01DA|
|LogTag North America Inc.|0x0AB7|
|Lone Star Marine Pty Ltd|0x0D3B|
|Long Range Systems, LLC|0x056C|
|Look Cycle International<br>Bluetooth SIG Proprietary|0x0D6E<br>P|



Page 385 of 442 

**Assigned Numbers** / Document 

**2026-06-23** 

|Loomanet, Inc.|0x0C02|
|---|---|
|Loop Devices, Inc|0x0302|
|Loopshore Oy|0x0779|
|Louis Vuitton|0x0A26|
|LoupeDeck Oy|0x07AC|
|Loy Tec electronics GmbH|0x0AA0|
|LS ELECTRIC Co., Ltd.|0x0EF5|
|LSC Control Systems Pty Ltd|0x104E|
|LSI ADL Technology|0x0270|
|LTIMINDTREE LIMITED|0x006A|
|Lucent|0x0007|
|LucentWear LLC|0x05CB|
|Lucimed|0x0901|
|Ludus Helsinki Ltd.|0x0084|
|LUGLOC LLC|0x04D6|
|Luidia Inc|0x0411|
|Lukoton Experience Oy|0x0215|
|lulabytes S.L.|0x037E|
|Lumen Labs (HK) Ltd|0x0F04|
|Lumen UAB|0x0529|
|LumenRadio AB|0x09E9|
|Lumens For Less, Inc|0x0756|
|Lumi United Technology Co., Ltd|0x0B27|
|LumiGeek LLC|0x0208|
|LUMINOAH, INC.|0x0D3E|
|Luminostics, Inc.|0x091B|
|Lumo Bodytech Inc.|0x0251|
|Lumos Health Inc.|0x0985|
|Luna Health, Inc.|0x0BEB|
|Luna XIO, Inc.|0x0736|
|Lunera Lighting Inc.|0x0528|
|Lupine|0x022D|
|Luster Leaf Products Inc|0x021F|
|Lutron Electronics Co., Inc.|0x04DE|
|Luxer Corporation|0x0A34|
|Luxottica Group S.p.A<br>Bluetooth SIG Proprietary|0x0D53<br>P|



Page 386 of 442 

**Assigned Numbers** / Document 

**2026-06-23** 

|Luxshare Precision Industry Co., Ltd.|0x0E4C|
|---|---|
|LVI Co.|0x0C1F|
|LX SOLUTIONS PTY LIMITED|0x0638|
|Lynxemi Pte Ltd|0x0493|
|LYS TECHNOLOGIES LTD|0x0581|
|Lytx, INC.|0x0A65|
|M-Way Solutions GmbH|0x024D|
|M3SH TECHNOLOGY INC.|0x1086|
|MA MICRO LIMITED|0x0EE1|
|MAATEL|0x0DBD|
|MAC SRL|0x0741|
|Machfu Inc.|0x0BAC|
|Macnica Inc.|0x020A|
|Macrogiga Electronics|0x059C|
|Macronix International Co. Ltd.|0x002C|
|MadgeTech, Inc|0x0CEE|
|MADS Inc|0x0132|
|MADSGlobalNZ Ltd.|0x0162|
|Maennl Elektronik GmbH|0x0F0D|
|MAERSK CONTAINER INDUSTRY A/S|0x0EE0|
|Magnitude Lighting Converters|0x030B|
|MagniWare Ltd.|0x0358|
|Magnus Technology Sdn Bhd|0x09DF|
|Mahr GmbH|0x06FE|
|MAINBOT|0x0B2B|
|Makichie Co., Ltd.|0x0EA9|
|makita corporation|0x0643|
|MakuSafe Corp|0x0D03|
|Mallow|0x10A9|
|Mammut Sports Group AG|0x0B82|
|MAMORIO.inc|0x0518|
|Mannkind Corporation|0x0680|
|Mansella Ltd|0x0021|
|Mantis Tech LLC|0x0761|
|Mantracourt Electronics Limited|0x04C3|
|Manus Machina BV|0x0220|



Bluetooth SIG Proprietary 

Page 387 of 442 

**Assigned Numbers** / Document 

**2026-06-23** 

|Map Large, Inc.|0x0A4A|
|---|---|
|MAQUET GmbH|0x0CA3|
|MARELLI EUROPE S.P.A.|0x00A9|
|Marquardt GmbH|0x0B05|
|Marshall Group AB|0x065A|
|Martin.Care GmbH|0x107C|
|Marvell Technology Group Ltd.|0x0048|
|Masbando GmbH|0x05FC|
|Masimo Corp|0x0243|
|Masonite Corporation|0x09E6|
|Master Lock|0x014B|
|Matisse Interactif inc.|0x1087|
|Matrix Inc.|0x035B|
|Mattel|0x03B6|
|Matting AB|0x0490|
|Maturix ApS|0x0D1A|
|Maven Machines, Inc.|0x0283|
|Maveric Automation LLC|0x0193|
|MAVERICK ENERGY SOLUTIONS INTERNATIONAL,INC|0x0F44|
|MAX-co., ltd|0x0B76|
|Maxell, Ltd.|0x0A19|
|Maxim Integrated Products|0x058B|
|MAXON INDUSTRIES, INC.|0x0C84|
|maxon motor ltd.|0x07FF|
|Maxscend Microelectronics Company Limited|0x03BA|
|Maytronics Ltd|0x0893|
|Maztech Industries, LLC|0x0CD7|
|MBARC LABS Inc|0x0715|
|MC10|0x00CA|
|McIntosh Group Inc|0x0C30|
|McLear Limited|0x04F6|
|MCOT INC.|0x0BC3|
|McWong International, Inc.|0x0CE6|
|Measurlogic Inc.|0x06C2|
|MEBSTER s.r.o.|0x0D62|
|MED-EL|0x0647|



Bluetooth SIG Proprietary 

Page 388 of 442 

**Assigned Numbers** / Document 

**2026-06-23** 

|Medallion Instrumentation Systems|0x05B1|
|---|---|
|Medela AG|0x0538|
|Medela, Inc|0x03A6|
|Media-Cartec GmbH|0x0BFE|
|MEDIATECH S.R.L.|0x074F|
|MediaTek, Inc.|0x0046|
|Medibound, Inc.|0x0E99|
|MEDIRLAB Orvosbiologiai Fejleszto Korlatolt Felelossegu Tarsasag|0x07F4|
|Medtronic Inc.|0x01F9|
|Megatronix (Beijing) Technology Co., Ltd|0x0E69|
|Megger Ltd|0x0CA6|
|Meggitt SA|0x0653|
|Meizhou Guo Wei Electronics Co., Ltd|0x0A57|
|Meizu Technology Co., Ltd.|0x03AB|
|Melange Systems Pvt. Ltd.|0x09E8|
|Melbot Studios, Sociedad Limitada|0x091E|
|MemCachier Inc.|0x042E|
|Mendeltron, Inc.|0x0C0C|
|Mequonic Engineering, S.L.|0x098B|
|Mercedes-Benz Group AG|0x017C|
|Mercury Marine, a division of Brunswick Corporation|0x0E86|
|Mereltron bv|0x0C0D|
|Merlinia A/S|0x0226|
|Merry Electronics (S) Pte Ltd|0x0D14|
|MERRY ELECTRONICS CO., LTD.|0x0D13|
|Mesh Systems LLC|0x0B50|
|Mesh-Net Ltd|0x014C|
|Meshtech AS|0x042D|
|Meshtronix Limited|0x0657|
|Meso international|0x00B6|
|Meta Platforms Technologies, LLC|0x058E|
|Meta Platforms, Inc.|0x01AB|
|Meta Watch Ltd.|0x00A3|
|MetaLogics Corporation|0x0537|
|Metanate Limited|0x0444|
|MetaSystem S.p.A.<br>Bluetooth SIG Proprietary|0x031F<br>P|



Page 389 of 442 

**Assigned Numbers** / Document 

**2026-06-23** 

|METER Group, Inc. USA|0x0498|
|---|---|
|Metormote AB|0x0368|
|Metronom Health Europe|0x0949|
|MewTel Technology Inc.|0x002F|
|Meyer Sound Laboratories, Incorporated|0x06D1|
|MG Energy Systems B.V.|0x0A5D|
|MGM WIRELESSS HOLDINGS PTY LTD|0x0C5C|
|MHL Custom Inc|0x05CA|
|micas AG|0x015A|
|Michael Parkin|0x0754|
|MiCommand Inc.|0x0063|
|Micro Technology Services, Inc.|0x0F5C|
|Micro-Design, Inc.|0x07D9|
|Microchip Technology Inc.|0x00CD|
|MICRODIA Ltd.|0x037D|
|Microoled|0x08F2|
|Micropower Group AB|0x0D33|
|Microsemi Corporation|0x04FF|
|Microsoft|0x0006|
|MICROSON S.A.|0x0A74|
|Microtronics Engineering GmbH|0x024E|
|Midmark|0x0CF1|
|Midwest Instruments & Controls|0x03B3|
|Miele & Cie. KG|0x0BC1|
|Mievo Technologies Private Limited|0x0CA9|
|Mighty Cast, Inc.|0x0147|
|Milestone AV Technologies LLC|0x06E1|
|Milwaukee Electric Tools|0x0165|
|Minami acoustics Limited|0x0CD9|
|Minda Corporation Ltd|0x10C0|
|MindMaze SA|0x0D7B|
|MindPeace Safety LLC|0x0714|
|MindRhythm, Inc.|0x0B3F|
|Mindspire Limited|0x1093|
|Minebea Access Solutions Inc.|0x0B0E|
|Minebea Intec GmbH|0x0DDA|



Bluetooth SIG Proprietary 

Page 390 of 442 

**Assigned Numbers** / Document 

**2026-06-23** 

|MinebeaMitsumi Inc.|0x062B|
|---|---|
|Minelab Electronics Pty Limited|0x01DE|
|Mini Solution Co., Ltd.|0x0458|
|MINIBREW HOLDING B.V|0x078C|
|MINIRIG|0x0E53|
|Minut, Inc.|0x07E5|
|MIRA, Inc.|0x03CF|
|Miridia Technology Incorporated|0x08DA|
|Misfit Wearables Corp|0x00DF|
|MISHIK Pte Ltd|0x0166|
|miSport Ltd.|0x02A8|
|Mist Systems, Inc.|0x0542|
|MistyWest Energy and Transport Ltd.|0x0A4B|
|MITACHI CO.,LTD.|0x0D78|
|Mitel Semiconductor|0x0010|
|MITSUBISHI ELECTRIC AUTOMATION (THAILAND) COMPANY LIMITED|0x0B66|
|Mitsubishi Electric Corporation|0x0014|
|MITSUBISHI ELECTRIC LIGHTING CO, LTD|0x0CA4|
|Mitsubishi Motors Corporation|0x0F4A|
|Mitutoyo Corporation|0x082A|
|MIV ELECTRONICS, LTD|0x0E93|
|MIWA LOCK CO.,Ltd|0x0543|
|MIYAKAWA ELECTRIC WORKS LTD.|0x0D29|
|MIYOSHI ELECTRONICS CORPORATION|0x047B|
|MIZUNO Corporation|0x0879|
|MML US, Inc|0x0D9F|
|MOBI7 TECNOLOGIA EM MOBILIDADE S.A.|0x0F90|
|Mobicomm Inc|0x021C|
|mobike (Hong Kong) Limited|0x04B3|
|Mobile Action Technology Inc.|0x09F6|
|MOBILE TECH, INC.|0x0E77|
|Mobile Technology Solutions LLC|0x0F3C|
|Mobilian Corporation|0x0037|
|Mobilogix|0x09E5|
|Mobiquity Networks Inc|0x0221|
|Mobitrace|0x09CA|



Bluetooth SIG Proprietary 

Page 391 of 442 

**Assigned Numbers** / Document 

**2026-06-23** 

|MOCA System Inc.|0x0B2E|
|---|---|
|Mode Lighting Limited|0x0661|
|MODRETRO, INC.|0x0F6D|
|MODULAR MEDICAL, INC.|0x0DD6|
|modum.io AG|0x05D7|
|Moeco IOT Inc.|0x07B2|
|MOKO TECHNOLOGY Ltd|0x0A62|
|Molex Corporation|0x039F|
|Molnlycke Health Care AB|0x0D65|
|Monadnock Systems Ltd.|0x08C7|
|Monarch International Inc.|0x0A91|
|Monidor|0x05A9|
|Monil AS|0x0EF3|
|Monitra SA|0x033E|
|MONNIT CORPORATION|0x109A|
|Monster, LLC|0x0070|
|Montage Connect, Inc.|0x0C05|
|Moonbird BV|0x0963|
|MooreSilicon Semiconductor Technology (Shanghai) Co., LTD.|0x0CD0|
|Mopeka Products LLC|0x0C7C|
|Morningstar Corporation|0x0DB7|
|MORNINGSTAR FX PTE. LTD.|0x0DC7|
|Morse Project Inc.|0x00F2|
|Moticon ReGo AG|0x08AE|
|Motion Instruments Inc.|0x05E6|
|Motionalysis, Inc.|0x0A42|
|Motiv, Inc.|0x036F|
|MOTIVE TECHNOLOGIES, INC.|0x036E|
|Motorola|0x0008|
|Motorola Solutions|0x04EC|
|MOTREX|0x0B8C|
|Motsai Research|0x0274|
|Movano Inc.|0x0D4F|
|Movella Technologies B.V.|0x0886|
|Movesense Oy|0x0C93|
|MQA Limited|0x0C00|



Bluetooth SIG Proprietary 

Page 392 of 442 

**Assigned Numbers** / Document 

**2026-06-23** 

|MS kajak7 UG (limited liability)|0x0E23|
|---|---|
|MSA Innovation, LLC|0x01A4|
|MSHeli s.r.l.|0x0237|
|MStar Semiconductor, Inc.|0x007A|
|Mstream Technologies., Inc.|0x0812|
|MTD Products Inc & Affiliates|0x08F7|
|MTG Co., Ltd.|0x0523|
|MTI Ltd|0x0216|
|Mueller-BBM Rail Technologies GmbH|0x104A|
|Muguang (Guangdong) Intelligent Lighting Technology Co., Ltd|0x0B88|
|Mul-T-Lock|0x0333|
|Multibit Oy|0x0290|
|Muoverti Limited|0x0229|
|Murata Manufacturing Co., Ltd.|0x013C|
|Musen Connect, Inc.|0x07A6|
|Mutrack Co., Ltd|0x0DF6|
|Muzik LLC|0x00DE|
|My Smart Blinds|0x0596|
|MYLAPS B.V.|0x06C9|
|mylife Diabetes Care AG|0x10C8|
|MYSPHERA|0x016C|
|mywerk system GmbH|0x0479|
|Myzee Technology|0x091F|
|NACHI-FUJIKOSHI CORP.|0x0DC1|
|NACON|0x0C82|
|NAGANO KEIKI CO., LTD.|0x1090|
|Nagravision SA|0x09DA|
|Nain Inc.|0x044B|
|Nalu Medical, Inc.|0x05C0|
|Namara Water Technologies, Inc|0x1042|
|Nanjing Qinheng Microelectronics Co., Ltd|0x07D7|
|Nanjing Xinxiangyuan Microelectronics Co., Ltd.|0x0D57|
|NanoFlex Power Corporation|0x0A28|
|Nanohex Corp|0x0D89|
|Nanoleaf Canada Limited|0x080B|
|Nanoleq AG<br>Bluetooth SIG Proprietary|0x0B1C<br>P|



Page 393 of 442 

**Assigned Numbers** / Document 

**2026-06-23** 

|NANOLINK APS|0x028C|
|---|---|
|NantSound, Inc.|0x0802|
|Naonext|0x08E8|
|NAOS JAPAN K.K.|0x0B15|
|Narhwall Inc.|0x09AD|
|Nathan Rhoades LLC|0x03DE|
|National Instruments|0x0554|
|Nations Technologies Inc.|0x0D16|
|Nature Inc.|0x0ECD|
|Nautilus Inc.|0x00F4|
|Navcast, Inc.|0x06DE|
|Navico|0x0F68|
|Naya Health, Inc.|0x035E|
|ndd Medizintechnik AG|0x0BD4|
|Neatebox Ltd|0x04BB|
|NEC Corporation|0x0022|
|NEC Lighting, Ltd.|0x0095|
|Nectar|0x0184|
|Nemik Consulting Inc|0x0456|
|Neo Materials and Consulting Inc.|0x0967|
|NEOKOHM SISTEMAS ELETRONICOS LTDA|0x0D96|
|NeoSensory, Inc.|0x07CF|
|Neosfar|0x02D8|
|NEOWRK SISTEMAS INTELIGENTES S.A.|0x0C01|
|Neptune First OU|0x0E79|
|Nerbio Medical Software Platforms Inc|0x0912|
|NeST|0x039A|
|Nest Labs Inc.|0x01B5|
|Nestlab AS|0x0FCF|
|Nestlé Nespresso S.A.|0x0225|
|NETATMO|0x0155|
|NetEase￿Hangzhou￿Network co.Ltd.|0x025C|
|NETGEAR, Inc.|0x0446|
|NETGRID S.N.C. DI BISSOLI MATTEO, CAMPOREALE SIMONE,<br>TOGNETTI FEDERICO|0x067F|
|Netizens Sp. z o.o.|0x01B1|



Bluetooth SIG Proprietary 

Page 394 of 442 

**Assigned Numbers** / Document 

**2026-06-23** 

|Neurable, Inc|0x10E1|
|---|---|
|NEURINNOV|0x0CAF|
|NeuroPace Inc|0x0EC9|
|Neurosity, Inc.|0x0BCE|
|Neurotherapeutics Ltd|0x0FCC|
|Neuvatek Inc.|0x0BC9|
|Nevro Corp.|0x0A31|
|New Audio LLC|0x099A|
|New Cosmos Electric Co., Ltd.|0x0E55|
|New Cosmos USA, Inc.|0x0BB4|
|New H3C Technologies Co.,Ltd|0x07AD|
|Newcon Optik|0x0559|
|Newlab S.r.l.|0x01D4|
|Newlogic|0x0017|
|NewNet, Inc.|0x1055|
|NewTec GmbH|0x05B0|
|Nexis Link Technology Co., Ltd.|0x0DBB|
|Nexite Ltd|0x0823|
|NexRev LLC|0x0ECA|
|NEXT DEVICES LTDA|0x0E67|
|Nextivity Inc.|0x0C88|
|NextMind|0x0997|
|Nextorage Corporation|0x103E|
|Nextscape Inc.|0x0A50|
|NextSense, Inc.|0x0E27|
|nFore Technology Co., Ltd.|0x0B44|
|NGK SPARK PLUG CO., LTD.|0x0A36|
|NIBROTECH LTD|0x0B25|
|Nice Spa|0x0FA4|
|NICHIEI INTEC CO., LTD.|0x0890|
|Nielsen-Kellerman|0x00EA|
|NIHON DENGYO KOUSAKU|0x06E6|
|NIHON KOHDEN CORPORATION|0x0E2E|
|NIKAT SOLUTIONS PRIVATE LIMITED|0x0D4E|
|Nike, Inc.|0x0078|
|Niko nv|0x05FE|



Bluetooth SIG Proprietary 

Page 395 of 442 

**Assigned Numbers** / Document 

**2026-06-23** 

|Nikon Corporation|0x0399|
|---|---|
|Nima Labs|0x03DC|
|Nimble Devices Oy|0x0129|
|Ninebot (Changzhou) Tech Co., Ltd.|0x0F1F|
|Ningbo Dooya Mechanic & Electronic Technology Co., Ltd|0x0F4E|
|NINGBO FOTILE KITCHENWARE CO., LTD.|0x0D50|
|NingBo klite Electric Manufacture Co.,LTD|0x0B64|
|NINGBO SHARKWARD ELECTRONICS CO.,LTD|0x0F34|
|Nintendo Co., Ltd.|0x0553|
|NIO USA, Inc.|0x0B48|
|Nippon Ceramic Co.,Ltd.|0x09A9|
|Nippon Seiki Co., Ltd.|0x0110|
|NIPPON SMT.CO.,Ltd|0x032C|
|NIPPON SYSTEMWARE CO.,LTD.|0x02D7|
|Niruha Systems Private Limited|0x077A|
|Nissan Motor Co., Ltd.|0x0BA6|
|Nisshinbo Micro Devices Inc.|0x085B|
|NITTO DENKO ASIA TECHNICAL CENTRE PTE. LTD.|0x0767|
|Nitto Denko Corporation|0x0E4A|
|NITTO KOGYO CORPORATION|0x0BAF|
|NIVELCO PROCESS CONTROL CO.|0x0FDA|
|Nixie Labs, Inc.|0x0372|
|NNOXX, Inc|0x0C61|
|NO CLIMB PRODUCTS LTD|0x0D55|
|NO SMD LIMITED|0x0919|
|Nobest Inc|0x0E8E|
|Noble HiFi, LLC|0x10BA|
|NOCTRIX HEALTH, INC|0x0EE6|
|Nod, Inc.|0x013E|
|NODER Joint Stock Company|0x0F36|
|Nofence AS|0x05AB|
|NOJA Power|0x0FCE|
|Noke|0x038B|
|Nokia Mobile Phones|0x0001|
|Nokian Renkaat Oyj|0x0896|
|Nomono AS|0x0B4A|



Bluetooth SIG Proprietary 

Page 396 of 442 

**Assigned Numbers** / Document 

**2026-06-23** 

|Noodle Technology inc|0x076C|
|---|---|
|Noomi AB|0x069C|
|Noordung d.o.o.|0x0621|
|Nordic Semiconductor ASA|0x0059|
|Nordic Strong ApS|0x0AD0|
|Noritz Corporation.|0x0C38|
|North Pole Engineering|0x0330|
|NorthStar Battery Company, LLC|0x040D|
|Norwood Systems|0x002E|
|NOTHING TECHNOLOGY LIMITED|0x0CCB|
|NOVABASE S.R.L.|0x07EB|
|NOVAFON - Electromedical devices limited liability company|0x0DCE|
|Novalogy LTD|0x0419|
|Novartis AG|0x052B|
|Novatel Wireless|0x0145|
|NOVEA ENERGIES|0x0B62|
|Novel Bits, LLC|0x08D3|
|Noventa AG|0x08C9|
|Novidan, Inc.|0x0A44|
|Novo Nordisk A/S|0x04CB|
|Novoferm tormatic GmbH|0x0DA7|
|Novotec Medical GmbH|0x0359|
|Nox Medical|0x03FB|
|NS Tech, Inc.|0x0522|
|NTEO Inc.|0x0138|
|NTT docomo|0x02E2|
|NTT sonority, Inc.|0x0C9B|
|NUANCE HEARING LTD|0x095F|
|Nubia Technology Co.,Ltd.|0x08CA|
|Nuheara Limited|0x043A|
|Numa Products, LLC|0x0CD5|
|Nura Operations Pty Ltd|0x0533|
|Nuviz, Inc.|0x045E|
|Nuvoton|0x0ACC|
|nVisti, LLC|0x049B|
|NXP B.V.|0x0025|



Bluetooth SIG Proprietary 

Page 397 of 442 

**Assigned Numbers** / Document 

**2026-06-23** 

|Nymbus, LLC|0x06A3|
|---|---|
|nymea GmbH|0x0AD6|
|Nymi Inc.|0x01B2|
|Nytec, Inc.|0x01B3|
|O. E. M. Controls, Inc.|0x05A4|
|O2 Micro, Inc.|0x0785|
|OBIQ Location Technology Inc.|0x06A0|
|ObjectSpectrum, LLC|0x10AB|
|Oblamatik AG|0x091C|
|Occly LLC|0x047D|
|OCEASOFT|0x0273|
|Oculeve, Inc.|0x0720|
|Odeon, Inc.|0x0D45|
|Odic Incorporated|0x086A|
|ODM Technology, Inc.|0x0096|
|OFF Line Co., Ltd.|0x051D|
|OFF Line Japan Co., Ltd.|0x0C1D|
|Off-Highway Powertrain Services Germany GmbH|0x0CE4|
|Offcode Oy|0x0B7E|
|OFIVE LIMITED|0x0E3A|
|Ohsung Electronics|0x0957|
|OJ Electronics A/S|0x0CF6|
|OJMAR SA|0x09EE|
|OKI Electric Industry Co., Ltd|0x09F5|
|Olibra LLC|0x0F03|
|OLIS ELECTRONICS, LLC|0x0EA3|
|Olumee|0x0B14|
|Olympic Ophthalmics, Inc.|0x0A6B|
|Olympus Corporation|0x04D0|
|OM Digital Solutions Corporation|0x09F1|
|Omegawave Oy|0x00AE|
|OMNI Remotes|0x057A|
|Omni-ID USA, INC.|0x0AC0|
|Omnivoltaic Energy Solutions Limited Company|0x0B37|
|OmniWave Microelectronics Shanghai Co., Ltd|0x0E5A|
|OMRON Corporation<br>Bluetooth SIG Proprietary|0x02D5<br>P|



Page 398 of 442 

**Assigned Numbers** / Document 

**2026-06-23** 

|Omron Healthcare Co., LTD|0x020E|
|---|---|
|ON Semiconductor|0x0362|
|onanoff limited|0x0F6F|
|OnAsset Intelligence, Inc.|0x0614|
|OnBeep|0x0151|
|ONCELABS LLC|0x0C44|
|OnePlus Electronics (Shenzhen) Co., Ltd.|0x072F|
|ONESPACE TECHNOLOGIES (PTY) LTD|0x0F22|
|OneSpan|0x02C8|
|ONKYO Corporation|0x062F|
|Onset Computer Corporation|0x00C5|
|ONWI|0x0E2F|
|OOBIK Inc.|0x0A94|
|Ooma|0x0A70|
|Oort Technologies LLC|0x02B7|
|Opal Camera, Inc.|0x0F30|
|OpConnect, Inc.|0x0E6F|
|Open Bionics Ltd.|0x0ABA|
|Open Interface|0x0027|
|Open Platform Systems LLC|0x072E|
|Open Research Institute, Inc.|0x064A|
|Open Road Solutions, Inc.|0x0CC5|
|Openbrain Technologies, Co., Ltd.|0x0113|
|OpenTech Alliance, Inc.|0x0E87|
|OPEX Corporation|0x0A41|
|OPICA GmbH|0x10C4|
|OPPLE Lighting Co., Ltd|0x0539|
|Optalert|0x0881|
|Optec, LLC|0x0D4D|
|Optek|0x0800|
|OPTEX CO.,LTD.|0x06E5|
|Optikam Tech Inc.|0x078B|
|OPTIMUSIOT TECH LLP|0x090E|
|Optivolt Labs, Inc.|0x0D8E|
|OPTRON Co., Ltd.|0x0EAD|
|OR Technologies Pty Ltd<br>Bluetooth SIG Proprietary|0x0747<br>P|



Page 399 of 442 

**Assigned Numbers** / Document 

**2026-06-23** 

|OrangeMicro Limited|0x0DD1|
|---|---|
|Oras Oy|0x0A08|
|ORB Innovations Ltd|0x0C3B|
|ORBIS Inc.|0x09D4|
|Orlan LLC|0x024B|
|Orpyx Medical Technologies Inc.|0x0C4C|
|ORSO Inc.|0x084C|
|OrthoAccel Technologies|0x041B|
|OrthoSensor, Inc.|0x0544|
|OS42 UG (haftungsbeschraenkt)|0x0616|
|OSKEY|0x106A|
|OSM HK Limited|0x0AAC|
|OSRAM GmbH|0x050C|
|Ossur hf.|0x0814|
|OTC engineering|0x0BE2|
|OTF Distribution, LLC|0x0BC8|
|OTF Product Sourcing, LLC|0x0C2D|
|OTL Dynamics LLC|0x00A5|
|Otodata Wireless Network Inc.|0x03B1|
|Otodynamics Ltd|0x0556|
|Otter Products, LLC|0x0206|
|OttoQ Inc|0x032F|
|Oura Health Ltd|0x0304|
|Oura Health Oy|0x02B2|
|OurHub Dev IvS|0x047E|
|Outshiny India Private Limited|0x0DE1|
|OV LOOP, INC.|0x05AF|
|Oval Corporation|0x0F15|
|Overhead Door Corporation|0x0ED9|
|ovrEngineered, LLC|0x04A2|
|Owl Labs Inc.|0x03F7|
|Owlet Baby Care Inc.|0x0E9F|
|Oxford Metrics plc|0x05BB|
|Ozo Edu, Inc.|0x03EB|
|P.I.Engineering|0x05C1|
|Pac Sane Limited|0x0A6E|



Bluetooth SIG Proprietary 

Page 400 of 442 

**Assigned Numbers** / Document 

**2026-06-23** 

|PaceBait IVS|0x0A2B|
|---|---|
|Pacific Bioscience Laboratories, Inc|0x04EA|
|Pacific Coast Fishery Services (2003) Inc.|0x0CEC|
|PACIFIC INDUSTRIAL CO., LTD.|0x0E32|
|Pacific Lock Company|0x02A4|
|PACIFIC MARINE BATTERIES PTY. LIMITED|0x0EC4|
|Pacific Track, LLC|0x086B|
|Packetcraft, Inc.|0x07E8|
|PAFERS TECH|0x0701|
|Pal Electronics|0x0C27|
|PAL Technologies Ltd|0x03F4|
|Palago AB|0x0469|
|Palarum LLC|0x10B8|
|PalatiumCare LLC|0x0F1B|
|PALRED RETAIL PRIVATE LIMITED|0x0FA1|
|Pambor Ltd.|0x046E|
|Pamex Inc.|0x0A95|
|Panasonic Automotive Systems Co., Ltd.|0x0E9B|
|Panasonic Holdings Corporation|0x003A|
|Panda Ocean Inc.|0x00A6|
|Panduit Corp.|0x065D|
|Parabit Systems, Inc.|0x05B3|
|Paradox Engineering SA|0x080F|
|Parakey AB|0x0C6F|
|Parker Hannifin Corp|0x0248|
|PARROT AUTOMOTIVE SAS|0x0043|
|Parsyl Inc|0x0857|
|Parthus Technologies Inc.|0x000E|
|Particle Industries, Inc.|0x0662|
|Passif Semiconductor Corp|0x00B0|
|PassiveBolt, Inc.|0x094E|
|PatchRx, Inc.|0x0E28|
|PAUL HARTMANN AG|0x0ABF|
|Paxton Access Ltd|0x0196|
|Paybuddy ApS|0x09A7|
|Payex Norge AS<br>Bluetooth SIG Proprietary|0x0658<br>P|



Page 401 of 442 

**Assigned Numbers** / Document 

**2026-06-23** 

|PayPal, Inc.|0x00F0|
|---|---|
|PayRange Inc.|0x02C9|
|PB INC.|0x0B1E|
|PCB Piezotronics, Inc.|0x0789|
|PCH International|0x0163|
|PCI Private Limited|0x092C|
|PEAG, LLC dba JLab Audio|0x0CE9|
|Pebble Technology|0x0154|
|Pedigree Technologies, LLC|0x10A7|
|PEEQ DATA|0x0311|
|PEG PEREGO SPA|0x03D4|
|Pegasus Technologies, Inc.|0x09B6|
|Pektron Group Limited|0x0853|
|Pella Corp|0x0D5E|
|Peloton Interactive Inc.|0x0768|
|PentaLock Aps.|0x09B4|
|Pepperl + Fuchs GmbH|0x0844|
|Perfect Company|0x0A8E|
|Performance Electronics, Ltd.|0x0C26|
|Perimeter Technologies, Inc.|0x0BCA|
|Personal Products Co., Ltd.|0x1094|
|Pertech Industries Inc|0x0B8D|
|PERUN TECH SP. Z O.O.|0x108B|
|Perytons Ltd.|0x0149|
|Peter Systemtechnik GmbH|0x00AD|
|petPOMM, Inc|0x03B5|
|Petronics Inc.|0x05DC|
|PetVoice Co., Ltd.|0x0D0E|
|Petzl|0x0296|
|PF SCHWEISSTECHNOLOGIE GMBH|0x08B2|
|Pharox B.V.|0x0FC7|
|Pharynks Corporation|0x022C|
|PHC Corporation|0x0726|
|phg Peter Hengstler GmbH + Co. KG|0x0C63|
|Phiaton Corporation|0x0C62|
|Philia Technology<br>Bluetooth SIG Proprietary|0x08E0<br>P|



Page 402 of 442 

**Assigned Numbers** / Document 

**2026-06-23** 

|Philip Morris Products S.A.|0x0223|
|---|---|
|Phillips Connect Technologies LLC|0x087F|
|Phillips-Medisize A/S|0x035A|
|PHONEPE PVT LTD|0x0629|
|PHOTODYNAMIC INCORPORATED|0x09AA|
|Photron Limited|0x0682|
|Phrame Inc.|0x061F|
|PHYPLUS Inc|0x0504|
|Physical Enterprises Inc.|0x0189|
|PhysioLogic Devices, Inc.|0x0D2A|
|Piaggio Fast Forward|0x083B|
|Pico Technology Inc.|0x03C4|
|Pieps GmbH|0x0351|
|PIKOLIN S.L.|0x0763|
|Pillsy Inc.|0x0433|
|Pinnacle Technology, Inc.|0x0A32|
|Pinpoint GmbH|0x0E07|
|Pinpoint Innovations Limited|0x0908|
|Pioneer Corporation|0x0150|
|Pirelli Tyre S.P.A.|0x04F5|
|PIRITIZ LLC|0x0F71|
|Piscisea Tech Inc|0x10C6|
|Pitius Tec S.L.|0x015C|
|Pitpatpet Ltd|0x0236|
|PixArt Imaging Inc.|0x065C|
|PIXEL TI IND. E COM PROD ELETRONICOS|0x0EE9|
|PIXELA CORPORATION|0x0DA5|
|Pixie Dust Technologies, Inc.|0x0998|
|Planmeca Oy|0x0D0C|
|PlantChoir Inc.|0x0799|
|Plantronics, Inc.|0x0055|
|Plastimold Products, Inc|0x08FF|
|PLAUD Inc.|0x0FB2|
|Playbrush|0x0254|
|PlayerData Limited|0x09B8|
|Playfinity AS<br>Bluetooth SIG Proprietary|0x0612<br>P|



Page 403 of 442 

**Assigned Numbers** / Document 

**2026-06-23** 

|Plejd AB|0x0377|
|---|---|
|Plume Design Inc|0x0A17|
|PLUS Location Systems Pty Ltd|0x0104|
|PMD Solutions|0x046B|
|Podo Labs, Inc|0x016F|
|POGS B.V.|0x0CEF|
|PointGuard, LLC|0x0966|
|Pointr Labs Limited|0x08D9|
|Pokkels|0x0A6C|
|Polar Electro Europe B.V.|0x00D1|
|Polar Electro OY|0x006B|
|Polaris IND|0x0501|
|Polidea Sp. z o.o.|0x08AF|
|Polk Audio|0x0F7C|
|Poly-Control ApS|0x01C8|
|Polycom, Inc.|0x059E|
|Polymap Wireless|0x0357|
|Polymorphic Labs LLC|0x0496|
|PONE BIOMETRICS AS|0x0BF3|
|Popit Oy|0x0973|
|Popper Pay AB|0x049F|
|Porsche AG|0x0120|
|POS Tuning Udo Vosshenrich GmbH & Co. KG|0x0482|
|Potrykus Holdings and Development LLC|0x081D|
|Powercast Corporation|0x02D3|
|Powerstick.com|0x0E70|
|Pozyx NV|0x09AE|
|PPRS|0x0951|
|PRADCO Outdoor Brands|0x0EB2|
|Praxis Dynamics|0x0222|
|Precision Outcomes Ltd|0x0382|
|Precision Triathlon Systems Limited|0x0B03|
|Precor|0x071B|
|PreEvnt, LLC|0x0F14|
|Preseed Japan Corporation|0x0EB5|
|Presidio Medical, Inc.|0x0B84|



Bluetooth SIG Proprietary 

Page 404 of 442 

**Assigned Numbers** / Document 

**2026-06-23** 

|PressurePro|0x05E7|
|---|---|
|Prestigio Plaza Ltd.|0x0137|
|Prevayl Limited|0x08BC|
|Prevent Biometrics|0x0347|
|Pricer AB|0x0CBD|
|Primax Electronics Ltd.|0x0E30|
|PRIMES GmbH|0x0EFD|
|Primus Inter Pares Ltd|0x01D0|
|Princess Cruise Lines, Ltd.|0x0CA5|
|Private limited company ”Teltonika”|0x089A|
|Pro-Mark, Inc.|0x047F|
|Procept|0x062E|
|Procon Analytics, LLC|0x0BD3|
|Procter & Gamble|0x00DC|
|prodigy|0x053B|
|Produal Oy|0x06AA|
|Prolojik Limited|0x063A|
|Prolon Inc.|0x0429|
|Promethean Ltd.|0x0126|
|Proof Diagnostics, Inc.|0x09A0|
|Propagation Systems Limited|0x03B2|
|Propeller Health|0x0378|
|Prosaris Solutions Ltd.|0x106F|
|PROSYS DEV LIMITED|0x0E02|
|PROTECH S.A.S. DI GIRARDI ANDREA & C.|0x055F|
|Protect Animals With Satellites LLC|0x0CFA|
|PS Engineering, Inc.|0x0AAE|
|PS GmbH|0x092E|
|PSA Peugeot Citroen|0x0A88|
|PSIKICK, INC.|0x0571|
|PSP - Pauli Services & Products GmbH|0x08F3|
|PSYONIC, Inc.|0x0882|
|PT SADAMAYA GRAHA TEKNOLOGI|0x0C7B|
|PUDSEY DIAMOND ENGINEERING LIMITED|0x0E4B|
|Puff Corp|0x0C03|
|Pulsate Mobile Ltd.|0x01BE|



Bluetooth SIG Proprietary 

Page 405 of 442 

**Assigned Numbers** / Document 

**2026-06-23** 

|Pune Scientific LLP|0x079F|
|---|---|
|Pur3 Ltd|0x0590|
|PURA SCENTS, INC.|0x0BD8|
|Puratap Pty Ltd|0x0BE6|
|Pure International Limited|0x0665|
|Q42 Internet B.V.|0x0EEF|
|Qblinks|0x016B|
|QIKCONNEX LLC|0x0EA2|
|Qingdao Eastsoft Communication Technology Co.,Ltd|0x0C86|
|Qingdao Haier Technology Co., Ltd.|0x0929|
|Qingdao Realtime Technology Co., Ltd.|0x046C|
|Qingdao Yeelink Information Technology Co., Ltd.|0x0164|
|Qingping Technology (Beijing) Co., Ltd.|0x0781|
|Qorvo Utrecht B.V.|0x0453|
|Qrio Inc|0x0235|
|QRITAGYA LLP|0x0FD4|
|QSC, LLC|0x0E4E|
|QT Medical INC.|0x05DE|
|Qualcomm|0x001D|
|Qualcomm Connected Experiences, Inc.|0x00D8|
|Qualcomm Innovation Center, Inc. (QuIC)|0x00B8|
|Qualcomm Labs, Inc.|0x011A|
|Qualcomm Life Inc|0x03E3|
|Qualcomm Technologies International, Ltd. (QTIL)|0x000A|
|Qualcomm Technologies, Inc.|0x00D7|
|QUANTATEC|0x0E44|
|Queclink Wireless Solutions Co., Ltd.|0x0E61|
|Quectel Wireless Solutions Co., Ltd.|0x0D87|
|Queercon, Inc|0x04D3|
|Quha oy|0x0B68|
|Quilt Systems, Inc.|0x0F60|
|Quintessential Design, Inc.|0x0ED6|
|Quintic Corp|0x008E|
|Quintrax Limited|0x0481|
|quip NYC Inc.|0x07F8|
|Qulinda AB|0x0F4D|



Bluetooth SIG Proprietary 

Page 406 of 442 

**Assigned Numbers** / Document 

**2026-06-23** 

|Quuppa Oy.|0x00C7|
|---|---|
|R F Micro Devices|0x0028|
|R-DAS, s.r.o.|0x0ABB|
|R.O. S.R.L.|0x09F8|
|R.W. Beckett Corporation|0x061A|
|RAB Lighting, Inc.|0x07A5|
|RACHIO, INC.|0x0D1B|
|Racketry, d. o. o.|0x0CB5|
|Radar Automobile Sales(Shandong)Co.,Ltd.|0x0C8F|
|Raden Inc|0x02B1|
|Radiance Technologies|0x0439|
|Radiawave Technologies Co.,Ltd.|0x090C|
|Radinn AB|0x07C2|
|Radio Sound|0x0CF3|
|Radio Systems Corporation|0x01FE|
|RadioPulse Inc|0x0597|
|Radioworks Microelectronics PTY LTD|0x0AF4|
|radius co., ltd.|0x077C|
|Radius Networks, Inc.|0x0118|
|Rafaelmicro|0x0864|
|RainMaker Solutions, Inc.|0x0E90|
|Ralink Technology Corporation|0x005B|
|Ranch Systems, Inc.|0x0FA9|
|RandomLab SAS|0x0699|
|RAPIDISE TECHNOLOGY PRIVATE LIMITED|0x10A4|
|rapitag GmbH|0x0598|
|Rashidov ltd|0x08E4|
|Raspberry Pi|0x1040|
|Ratio Electric BV|0x0BFF|
|RATOC Systems, Inc.|0x0B60|
|Raven Industries|0x023E|
|Raycon Inc.|0x10AE|
|Rayden.Earth LTD|0x08C1|
|Raytac Corporation|0x068A|
|Razer Inc.|0x068E|
|RB Controls Co., Ltd.<br>Bluetooth SIG Proprietary|0x0515<br>P|



Page 407 of 442 

**Assigned Numbers** / Document 

**2026-06-23** 

|RDA Microelectronics|0x0061|
|---|---|
|React Accessibility Limited|0x07D2|
|React Mobile|0x0B6E|
|REACTEC LIMITED|0x04E1|
|READY FOR SKY LLP|0x0BC0|
|Real Time Automation, Inc.|0x045F|
|Real-World-Systems Corporation|0x05BF|
|Realityworks, inc.|0x0AA6|
|Realme Chongqing Mobile Telecommunications Corp., Ltd.|0x08A4|
|RealMega Microelectronics technology (Shanghai) Co. Ltd.|0x0AC2|
|Realtek Semiconductor Corporation|0x005D|
|RealThingks GmbH|0x0937|
|REALTIMEID AS|0x0B3D|
|Reconnect, Inc.|0x0935|
|Red 100 Lighting Co., ltd.|0x0B39|
|Red-M (Communications) Ltd|0x0032|
|REDARC ELECTRONICS PTY LTD|0x0B2D|
|Redmond Industrial Group LLC|0x056D|
|Redpine Signals Inc|0x06FF|
|REEKON TOOLS INC.|0x0DF4|
|Reelables, Inc.|0x0ADB|
|Reflow Pty Ltd|0x0A07|
|Refrigerated Transport Electronics, Inc.|0x0D49|
|Regal Beloit America, Inc.|0x0CE1|
|Regent Beleuchtungskorper AG|0x07A0|
|REGULA Ltd.|0x07BF|
|Relations Inc.|0x0401|
|Relish Technologies Limited|0x0ED5|
|REMDEVICE S.R.L.|0x0FD9|
|Remedee Labs|0x07DB|
|Remote Solution Co., LTD.|0x075F|
|Remoticom B.V.|0x1069|
|Renault SA|0x07FC|
|Renesas Design Netherlands B.V.|0x00D2|
|Renesas Electronics Corporation|0x0036|
|Renishaw PLC|0x0655|



Bluetooth SIG Proprietary 

Page 408 of 442 

**Assigned Numbers** / Document 

**2026-06-23** 

|Reoqoo IoT Technology Co., Ltd.|0x0CD4|
|---|---|
|Republic Wireless, Inc.|0x08E3|
|Research Products Corporation|0x0D0B|
|RESIDEO TECHNOLOGIES, INC.|0x0B01|
|Resmed Ltd|0x038D|
|Resolution Products, Ltd.|0x0134|
|Respiri Limited|0x0713|
|Revenue Collection Systems FRANCE SAS|0x0641|
|Revol Technologies Inc|0x0484|
|REVSMART WEARABLE HK CO LTD|0x071A|
|Revvo Technologies, Inc.|0x086C|
|RF Code, Inc.|0x0286|
|RF Creations|0x0CB1|
|RF Digital Corp|0x028E|
|RF Electronics Limited|0x0D8D|
|RF INNOVATION|0x0457|
|RHA TECHNOLOGIES LTD|0x0664|
|Rheem Sales Company, Inc.|0x0C56|
|RHOMBUS SYSTEMS, INC.|0x075D|
|Ribbiot, INC.|0x0C9D|
|RICKARD AIR DIFFUSION (PTY) LTD|0x0EE7|
|Ricoh Company Ltd|0x065F|
|RIDE VISION LTD|0x0A16|
|Rigado LLC|0x0202|
|RIGH, INC.|0x0E6C|
|RIIG AI Sp. z o.o.|0x0314|
|RIKEN KEIKI CO., LTD.,|0x0834|
|Rinnai Corporation|0x02B9|
|Rion Co., Ltd.|0x056B|
|Rivata, Inc.|0x0A83|
|Rivian Automotive, LLC|0x0941|
|Rivieh, Inc.|0x0B5F|
|RivieraWaves S.A.S|0x0060|
|RJ Brands LLC|0x05CD|
|RM Acquisition LLC|0x04D9|
|Roam Devices LLC|0x0F6A|



Bluetooth SIG Proprietary 

Page 409 of 442 

**Assigned Numbers** / Document 

**2026-06-23** 

|Roambee Corporation|0x06A6|
|---|---|
|Roambotics, Inc.|0x0BAD|
|Robert Bosch GmbH|0x02A6|
|Robkoo Information & Technologies Co., Ltd.|0x0ABE|
|ROBSON SRL|0x0F5F|
|Roca Sanitario, S.A.|0x0B1A|
|Roche Diabetes Care AG|0x0170|
|Rochester Sensors, LLC|0x0C7F|
|Rockchip Electronics Co., Ltd.|0x0C1B|
|Rockford Corp.|0x03F8|
|Rockpile Solutions, LLC|0x0D4A|
|ROCKWELL AUTOMATION, INC.|0x0D86|
|Rocky Mountain ATV/MC Jake Wilson|0x0D02|
|Rocky Radios LLC|0x0E74|
|Rocoto Ltd|0x0E5C|
|Rohde & Schwarz GmbH & Co. KG|0x0019|
|Rokk Limited|0x0F21|
|Rokstar Holdings Limited|0x106D|
|Roku, Inc.|0x07A2|
|ROL Ergo|0x0153|
|Roland Corporation|0x0A0E|
|Rollease Acmeda, Inc.|0x0FBB|
|Romcor Pty Ltd|0x1054|
|Rookery Technology Ltd|0x0607|
|ROOQ GmbH|0x0927|
|RORENTECH Co., Ltd.|0x0F17|
|Rosewill|0x09C1|
|Rotarex S.A.|0x1044|
|Rotor Bike Components|0x0323|
|Rotronic AG|0x0B89|
|ROYAL PARTS CO.,LTD|0x0F8C|
|RSA Security LLC|0x1057|
|RTC Industries, Inc.|0x0660|
|RTX A/S|0x0015|
|Runteq Oy Ltd|0x02F8|
|Runtime, Inc.<br>Bluetooth SIG Proprietary|0x05C3<br>P|



Page 410 of 442 

**Assigned Numbers** / Document 

**2026-06-23** 

|Ruptela|0x0E5F|
|---|---|
|Ruuvi Innovations Ltd.|0x0499|
|ruwido austria gmbh|0x027F|
|Rx Networks, Inc.|0x02AD|
|Ryeex Technology Co.,Ltd.|0x0649|
|RYSE INC.|0x0409|
|S-Labs Sp. z o.o.|0x048D|
|S-Power Electronics Limited|0x00BB|
|SAAB Aktiebolag|0x0BBE|
|SABIK Offshore GmbH|0x0678|
|safectory GmbH|0x0A35|
|SAFEGUARD EQUIPMENT, INC.|0x0DB6|
|SafeLine Sweden AB|0x06EA|
|SafeNow GmbH|0x0F64|
|SafePort|0x084D|
|Safera Oy|0x072D|
|Safetech Products LLC|0x04CD|
|Safety Lighting Research Pty Ltd|0x104F|
|Safety Swim LLC|0x0DEE|
|Safetytest GmbH|0x0BEF|
|SainStore Technology Co., Ltd.|0x10C2|
|SALTO SYSTEMS S.L.|0x0199|
|SaluStim Group Oy|0x0BB9|
|Salutica Allied Solutions|0x0127|
|Salyx Medical Inc.|0x0F3E|
|Sam Labs Ltd.|0x01CC|
|Samsara Networks, Inc|0x0B6B|
|Samsung Electronics Co. Ltd.|0x0075|
|Samsung SDS Co., Ltd.|0x02DE|
|Sanistaal A/S|0x0B0B|
|Sankyo Air Tech Co.,Ltd.|0x0B79|
|SANlight GmbH|0x0A8B|
|Sanofi|0x0743|
|SANWA NEWTEC CO.,LTD.|0x0F3B|
|SANYO DENKO Co.,Ltd.|0x0B0D|
|Sapphire Circuits LLC<br>Bluetooth SIG Proprietary|0x0250<br>P|



Page 411 of 442 

**Assigned Numbers** / Document 

**2026-06-23** 

|Sappl Verwaltungs- und Betriebs GmbH|0x092A|
|---|---|
|Saris Cycling Group, Inc|0x00B1|
|Sarita CareTech APS|0x0560|
|Sartorius AG|0x06FB|
|Sarvavid Software Solutions LLP|0x074B|
|Saucon Technologies|0x089B|
|Savant Systems LLC|0x01D9|
|Savitech Corp.,|0x053A|
|SAVOY ELECTRONIC LIGHTING|0x09B9|
|Saxonar GmbH|0x0AED|
|SB C&S Corp.|0x0CC7|
|Scale-Tec, Ltd|0x064B|
|Scanbro OU|0x0E9A|
|Scandinavian Health Limited|0x0C13|
|Scangrip A/S|0x0B7C|
|SCARAB SOLUTIONS LTD|0x0C70|
|Schawbel Technologies LLC|0x0266|
|SCHELL GmbH & Co. KG|0x0DD9|
|Schneider Electric|0x02B6|
|Schneider Schreibgeräte GmbH|0x024F|
|SchoolBoard Limited|0x039E|
|Schrader Electronics|0x0601|
|Schueco International KG|0x0F37|
|Schulte-Schlagbaum AG|0x0EBF|
|SCIENTERRA LIMITED|0x0D20|
|sclak s.r.l.|0x088A|
|SCM Group|0x0B81|
|Scope Logistical Solutions|0x0906|
|Scosche Industries, Inc.|0x0791|
|Screenovate Technologies Ltd|0x053C|
|Scribble Design Inc.|0x0A6A|
|Scuf Gaming International, LLC|0x057F|
|SDATAWAY|0x0395|
|Sears Holdings Corporation|0x0402|
|SEAT es|0x0125|
|Seaward Electronic|0x0EF0|



Bluetooth SIG Proprietary 

Page 412 of 442 

**Assigned Numbers** / Document 

**2026-06-23** 

|seca GmbH & Co. KG|0x0CB9|
|---|---|
|SECOM CO., LTD.|0x0442|
|Security Enhancement Systems, LLC|0x09A5|
|Secuyou ApS|0x02D4|
|SECVRE GmbH|0x0261|
|Seekwave Technology Co.,ltd.|0x0C4D|
|Seers Technology Co., Ltd.|0x007D|
|SeeScan|0x05F3|
|SEFAM|0x0412|
|Seguro Technology Sp. z o.o.|0x02BE|
|Seibert Williams Glass, LLC|0x04C5|
|Seiko Epson Corporation|0x0040|
|Seiko Future Creation Inc.|0x0F9F|
|Seiko Instruments Inc.|0x0B8A|
|SEIKOIST, INC|0x0F96|
|Seitec Elektronik GmbH|0x0746|
|Seitron Spa|0x0F39|
|Selco GmbH|0x1099|
|Selfly BV|0x00C6|
|SELVE GmbH & Co. KG|0x05C5|
|Semilink Inc|0x00E2|
|Sena Technologies Inc.|0x0960|
|Sencilion Oy|0x06B4|
|Send Solutions|0x02D6|
|Sendum Wireless Corporation|0x099B|
|Sengled Co., Ltd.|0x08B4|
|Senic Inc.|0x0C09|
|SenionLab AB|0x01BC|
|SENNHEISER electronic GmbH & Co. KG|0x0494|
|Senops Tracker, LLC|0x10B1|
|SENOSPACE LLC|0x0CCE|
|Senquip Pty Ltd|0x0A71|
|SENS Innovation ApS|0x061D|
|Sens.ai Incorporated|0x0AB8|
|Sensa LLC|0x0C6E|
|SENSATEC Co., Ltd.<br>Bluetooth SIG Proprietary|0x09F7<br>P|



Page 413 of 442 

**Assigned Numbers** / Document 

**2026-06-23** 

|Senscomm Semiconductor Co., Ltd.|0x0B8F|
|---|---|
|Sensear Pty Ltd|0x0EFA|
|Senseonics, Incorporated|0x0F75|
|SenseQ Inc.|0x06AE|
|SenseWorks Tecnologia Ltda.|0x0E0F|
|Sensirion AG|0x06D5|
|Sensitech, Inc.|0x0B6C|
|Senso4s d.o.o.|0x09CC|
|Sensoan Oy|0x03E2|
|Sensogram Technologies, Inc.|0x02E9|
|Sensolus|0x0A11|
|Sensome|0x054D|
|Sensoria Holdings LTD|0x0B9D|
|Sensority, s.r.o.|0x077D|
|Sensormate AG|0x0BE8|
|Sensorworx, Inc.|0x1053|
|Sensoryx AG|0x0D2B|
|Sensoteq Ltd|0x0F55|
|Sensovium Inc.|0x08D1|
|Sensovo GmbH|0x0E1A|
|Sentek Pty Ltd|0x0B61|
|Senti Technologies Private Limited|0x0FA8|
|Sentrax GmbH|0x0B41|
|SentriLock|0x0176|
|Sera4 Ltd.|0x02A2|
|Seraphim Sense Ltd|0x0187|
|SERENE GROUP, INC|0x07F2|
|Serial Technology Corporation|0x0CC6|
|Server Products, Inc.|0x0EB6|
|Server Technology Inc.|0x00EB|
|Sesam Solutions BV|0x065B|
|Sesame AI, Inc.|0x1088|
|Setec Pty Ltd|0x051F|
|SetPoint Medical|0x036A|
|SEW-EURODRIVE GmbH & Co KG|0x0D85|
|SFS unimarket AG|0x0899|



Bluetooth SIG Proprietary 

Page 414 of 442 

**Assigned Numbers** / Document 

**2026-06-23** 

|SG Armaturen AS|0x0EE8|
|---|---|
|SGL Italia S.r.l.|0x0310|
|SGV Group Holding GmbH & Co. KG|0x0646|
|ShadeCraft, Inc|0x06B8|
|Shake-on B.V.|0x050A|
|Shanghai All Link Microelectronics Co.,Ltd|0x0AF2|
|Shanghai Flyco Electrical Appliance Co., Ltd.|0x04C8|
|Shanghai Frequen Microelectronics Co., Ltd.|0x02FC|
|shanghai fudan electronics group company Co. Ltd|0x0F5B|
|Shanghai high-flying electronics technology Co.,Ltd|0x0A99|
|Shanghai Holychip Electronic Co., Ltd.|0x1043|
|Shanghai InGeek Cyber Security Co., Ltd.|0x0759|
|Shanghai Kfcube Inc|0x08A8|
|Shanghai Mountain View Silicon Co.,Ltd.|0x06D9|
|Shanghai MXCHIP Information Technology Co., Ltd.|0x0922|
|Shanghai Panchip Microelectronics Co., Ltd|0x07D1|
|Shanghai Smart System Technology Co., Ltd|0x0A87|
|Shanghai Suisheng Information Technology Co., Ltd.|0x0828|
|ShangHai Super Smart Electronics Co. Ltd.|0x0072|
|Shanghai Top-Chip Microelectronics Tech. Co., LTD|0x07CE|
|Shanghai wuqi microelectronics Co.,Ltd|0x0A06|
|Shanghai Xiaoyi Technology Co.,Ltd.|0x05A1|
|Shanghai Yidian Intelligent Technology Co., Ltd.|0x0A0C|
|ShapeLog, Inc.|0x07EA|
|SHAPER TOOLS, INC.|0x0E5E|
|Sharknet srl|0x0541|
|SharkNinja Operating LLC|0x0C4F|
|Sharp Corporation|0x0381|
|Shearwater Research Inc.|0x1064|
|SHENZHEN AUKEY E BUSINESS CO., LTD|0x08DC|
|Shenzhen Baseus Technology Co., Ltd.|0x0E66|
|SHENZHEN BESTWAY ELECTRONICS CO.,LTD|0x0ED2|
|ShenZhen BoYiChuangXin|0x0EDB|
|Shenzhen CESI Information Technology Co., Ltd.|0x0CCF|
|SHENZHEN CHENYUN ELECTRONICS CO., LTD|0x0CDF|
|SHENZHEN CHINA MICRO SEMICON CO.,LIMITED<br>Bluetooth SIG Proprietary|0x10B6<br>P|



Page 415 of 442 

**Assigned Numbers** / Document 

**2026-06-23** 

|Shenzhen Chuangyuan Digital Technology Co., Ltd|0x0DB8|
|---|---|
|Shenzhen Conex|0x081A|
|Shenzhen CoolKit Technology Co., Ltd|0x0ACA|
|Shenzhen Cyber Innovation Technology Co., Ltd.|0x0EF6|
|SHENZHEN DIGITECH CO., LTD|0x0E1C|
|SHENZHEN DNS INDUSTRIES CO., LTD.|0x0DDE|
|ShenZhen Doctors of Intelligence & Technology Co.,Ltd|0x0F10|
|Shenzhen DOKE Electronic Co., Ltd|0x0D47|
|Shenzhen EBELONG Technology Co., Ltd.|0x0DAA|
|Shenzhen ECO-NEWLEAF CO., LTD|0x10C1|
|Shenzhen eMeet technology Co.,Ltd|0x0E03|
|Shenzhen Excelsecu Data Technology Co.,Ltd|0x00C1|
|Shenzhen Feasycom Technology Co., Ltd.|0x0A2D|
|Shenzhen FHX Precision Hardware & Plastic Co., Ltd.|0x1067|
|shenzhen fitcare electronics Co.,Ltd|0x082B|
|Shenzhen Goodix Technology Co., Ltd|0x04F7|
|Shenzhen Goodocom Information Technology Co., Ltd.|0x0E6B|
|Shenzhen Grandsun Electronic Co.,Ltd.|0x0A90|
|SHENZHEN GUANG QI GUO CHUANG TECHNOLOGY CO.,LTD|0x0F7A|
|Shenzhen Guo-link Technology Co.,Ltd.|0x0F58|
|SHENZHEN GWSTAI TECHNOLOGY CO.,LTD|0x0F05|
|Shenzhen H&T Intelligent Control Co., Ltd|0x0A2C|
|Shenzhen Hali-Power Industrial Co., Ltd.|0x109F|
|shenzhen Holyiot Technology Co.,Ltd|0x0F7E|
|shenzhen hongever technology Co,. Ltd|0x0EE2|
|Shenzhen Huasheng Jiaye Technology Co., Ltd|0x1041|
|Shenzhen ImagineVision Technology Limited|0x0B5B|
|Shenzhen iMCO Electronic Technology Co.,Ltd|0x0379|
|Shenzhen Injoinic Technology Co., Ltd.|0x0BD6|
|Shenzhen Jahport Electronic Technology Co., Ltd.|0x0DC0|
|Shenzhen Jimi loT Co.,Ltd|0x0FC3|
|Shenzhen Jingxun Technology Co., Ltd.|0x0AC1|
|SHENZHEN KAADAS INTELLIGENT TECHNOLOGY CO.,Ltd|0x0B53|
|Shenzhen KTC Technology Co.,Ltd.|0x0B7A|
|SHENZHEN LEMONJOY TECHNOLOGY CO., LTD.|0x03E9|
|Shenzhen Lunci Technology Co., Ltd<br>Bluetooth SIG Proprietary|0x0DDF<br>P|



Page 416 of 442 

**Assigned Numbers** / Document 

**2026-06-23** 

|Shenzhen Matches IoT Technology Co., Ltd.|0x0DFA|
|---|---|
|Shenzhen Minew Technologies Co., Ltd.|0x0639|
|Shenzhen MODSEMI Co., Ltd|0x0DE3|
|Shenzhen Mutual Technology Co., Ltd|0x0FB9|
|Shenzhen Muyu Technology Co., Ltd.|0x1077|
|Shenzhen NEOECO Technology Co., Ltd.|0x0ECC|
|Shenzhen Openhearing Tech CO., LTD .|0x0CAD|
|Shenzhen Poseidon Network Technology Co., Ltd|0x0CAA|
|SHENZHEN POWEROAK NEWENER CO., LTD|0x0F06|
|Shenzhen Qianfenyi Intelligent Technology Co., LTD|0x0B08|
|SHENZHEN REFLYING ELECTRONIC CO., LTD|0x0DB3|
|Shenzhen Shokz Co.,Ltd.|0x0CAC|
|Shenzhen Shuye Technology Co.,Ltd.|0x1059|
|Shenzhen Simo Technology co. LTD|0x08E2|
|SHENZHEN SOUNDSOUL INFORMATION TECHNOLOGY CO.,LTD|0x0E00|
|Shenzhen SuLong Communication Ltd|0x0233|
|Shenzhen Sunricher Technology Limited|0x0A78|
|Shenzhen SuperSound Technology Co.,Ltd|0x0F19|
|Shenzhen Tingting Technology Co. LTD|0x0CEB|
|Shenzhen TonliScience and Technology Development Co.,Ltd|0x07F6|
|Shenzhen Uascent Technology Co., Ltd|0x0AA5|
|Shenzhen Xinfeiyi Technology Co., Ltd.|0x0F48|
|Shenzhen Xunzong Zhilian Technology Co., Ltd.|0x10B2|
|Shenzhen Yopeak Optoelectronics Technology Co., Ltd.|0x0A3F|
|Shenzhen Yunke Intelligent Co.Ltd|0x0F52|
|Shenzhen Zhongguang Infotech Technology Development Co., Ltd|0x06CA|
|Shenzhen Zoqin Technology Co., Ltd.|0x0EDC|
|SHIBAURA ELECTRONICS CO., LTD.|0x109E|
|Shibutani Co., Ltd.|0x06F1|
|SHIMANO INC.|0x044A|
|Shindengen Electric Manufacturing Co., Ltd.|0x0BE9|
|SHINKAWA Sensor Technology, Inc.|0x0CB2|
|Shoof Technologies|0x06AF|
|Shortcut Labs|0x030F|
|Shure Inc|0x04AD|
|SIA Mesh Group<br>Bluetooth SIG Proprietary|0x0CD8<br>P|



Page 417 of 442 

**Assigned Numbers** / Document 

**2026-06-23** 

|SIANA Systems|0x0A0B|
|---|---|
|Sibel Inc.|0x09ED|
|Sichuan Changhong Neonet Technologies Co.,Ltd.|0x0F25|
|SiChuan Homme Intelligent Technology co.,Ltd.|0x0D5F|
|SICK AG|0x0DA0|
|Siemens AG|0x022E|
|SiFli Technologies (shanghai) Inc.|0x0A4C|
|SIG SAUER, INC.|0x0B17|
|Sigenergy Technology Co., Ltd.|0x10AA|
|Sigma Connectivity AB|0x03D3|
|Sigma Designs, Inc.|0x0355|
|SignalFire Telemetry, Inc.|0x0D40|
|SignalQuest, LLC|0x0C37|
|Signia Technologies, Inc.|0x001B|
|Signify Netherlands B.V.|0x060F|
|Signtle Inc.|0x0BC7|
|SIGNUM INTELLIGENCE LTD|0x0F8F|
|SIKOM AS|0x05EF|
|SIL System Integration Laboratory GmbH|0x0D2C|
|Silergy Corp|0x053F|
|silex technology, inc.|0x061B|
|Silicon Laboratories|0x02FF|
|Silicon Vandals PTY LTD|0x0F59|
|Silicon Wave|0x000B|
|Silk Labs, Inc.|0x0432|
|Silvair, Inc.|0x0136|
|SILVER TREE LABS, INC.|0x0B97|
|Silver Wolf Vehicles Inc.|0x0B31|
|Silverlake Technologies|0x0F57|
|SilverPlus, Inc|0x0540|
|simatec ag|0x0F76|
|Simm Tronic Limited|0x06C6|
|SimpliSafe, Inc.|0x06B1|
|Simply Embedded Inc.|0x0D8A|
|SING SUN TECHNOLOGY (INTERNATIONAL) LIMITED|0x0DEF|
|Sinnoz|0x054F|



Bluetooth SIG Proprietary 

Page 418 of 442 

**Assigned Numbers** / Document 

**2026-06-23** 

|Sino Wealth Electronic Ltd.|0x0121|
|---|---|
|Sinosun Technology Co., Ltd.|0x047A|
|SINTEF|0x0B36|
|SIRC Co., Ltd.|0x08BF|
|SiRF Technology, Inc.|0x0050|
|SIRONA Dental Systems GmbH|0x0D3C|
|SISTEMAS KERN, SOCIEDAD ANÓMINA|0x0808|
|Site IQ LLC|0x0BF1|
|Siteco GmbH|0x0A3C|
|SITERWELL ELECTRONICS CO.,LIMITED|0x0FB8|
|Situne AS|0x049A|
|SIVA Inotec Limited|0x0FD8|
|Sivantos GmbH|0x0295|
|Six Guys Labs, s.r.o.|0x0619|
|SK Telecom|0x0289|
|SKAIChips Co.,Ltd|0x10DA|
|SKC Inc|0x0815|
|Skeed,co,Ltd.|0x0E96|
|Skewered Fencing, LLC|0x0E88|
|SKF (U.K.) Limited|0x040E|
|SKF France|0x0D63|
|SKIDATA AG|0x0485|
|Skoda Auto a.s.|0x011E|
|Skullcandy, Inc.|0x07C9|
|Sky UK Limited|0x079C|
|Sky Wave Design|0x01D3|
|SkyCell AG|0x108F|
|SkyHawke Technologies|0x0B49|
|SKYROAM, INC.|0x0F7B|
|SkyStream Corporation|0x09BB|
|Skytech Creations Limited|0x0D9C|
|Skywalk Inc.|0x0F6B|
|SLOC GmbH|0x0E38|
|SmallLoop, LLC|0x0428|
|Smart Component Technologies Limited|0x0535|
|Smart Parks B.V.|0x0A61|



Bluetooth SIG Proprietary 

Page 419 of 442 

**Assigned Numbers** / Document 

**2026-06-23** 

|Smart Products Connection, S.A.|0x0D60|
|---|---|
|Smart Sensor Devices AB|0x075B|
|Smart Solution Technology, Inc.|0x04E6|
|Smart Technologies and Investment Limited|0x0549|
|Smart Wave Technologies Canada Inc|0x07CD|
|SMART-INNOVATION.inc|0x02EF|
|smart-TEC GmbH & Co. KG|0x1098|
|SMARTD TECHNOLOGIES INC.|0x0C24|
|SmartDrive|0x0862|
|Smartifier Oy|0x00F5|
|Smartloxx GmbH|0x079E|
|SmartMovt Technology Co., Ltd|0x03A3|
|SmartResQ ApS|0x0876|
|SmartSensor Labs Ltd|0x0861|
|SmartSnugg Pty Ltd|0x070D|
|SmartWireless GmbH & Co. KG|0x0891|
|SMK Corporation|0x0265|
|SMT ELEKTRONIK GmbH|0x0AFB|
|Snap-on Incorporated|0x07DF|
|Snapchat Inc|0x03C2|
|SNAPPWISH LLC|0x0E35|
|SnapStyk Inc.|0x0430|
|SNIFF LOGIC LTD|0x0C6C|
|Snowball Technology Co., Ltd.|0x0A85|
|Snuza (Pty) Ltd|0x00DB|
|Sociometric Solutions, Inc.|0x06B6|
|Société des Produits Nestlé S.A.|0x037F|
|Socket Mobile|0x0044|
|SOCOMEC|0x0744|
|SODA GmbH|0x0416|
|Software Development, LLC|0x0D8B|
|SOJI ELECTRONICS JOINT STOCK COMPANY|0x0DFC|
|Solabs Technology (HK) Co., Limited|0x10B4|
|Soliton Systems K.K.|0x088D|
|SOLUM CO., LTD|0x0B6D|
|SOLUTIONS AMBRA INC.|0x07D8|



Bluetooth SIG Proprietary 

Page 420 of 442 

**Assigned Numbers** / Document 

**2026-06-23** 

|SOLUX PTY LTD|0x0CF4|
|---|---|
|Soma Labs LLC|0x0BAE|
|SOMFY SAS|0x05C8|
|SomnoMed Limited|0x0C17|
|Sonas, Inc.|0x0CA8|
|SONICOS ENTERPRISES, LLC|0x0BA5|
|SonicSensory Inc|0x097E|
|SONO ELECTRONICS. CO., LTD|0x0320|
|Sonos Inc|0x05A7|
|Sonova AG|0x0282|
|Sonova Consumer Hearing GmbH|0x0BA3|
|Sontheim Industrie Elektronik GmbH|0x0A5A|
|Sony Corporation|0x012D|
|Sony Ericsson Mobile Communications|0x0056|
|Sony Honda Mobility Inc.|0x0EDE|
|Soprod SA|0x0512|
|Soraa Inc.|0x04B8|
|SOREX - Wireless Solutions GmbH|0x0491|
|Sospitas, s.r.o.|0x04C1|
|Sound ID|0x006F|
|SOUNDBOKS|0x0858|
|Soundbrenner Limited|0x07B6|
|Sounding Audio Industrial Ltd.|0x0E0B|
|Soundmax Electronics Limited|0x06A9|
|SOUNDUCT|0x0E46|
|Soundwave Hearing, LLC|0x0D4B|
|South Silicon Valley Microelectronics|0x039B|
|Southco|0x0D64|
|Southern Audio Services, Inc|0x0F3A|
|Southwire Company, LLC|0x0A05|
|Spacelabs Medical Inc.|0x08F9|
|Span.IO, Inc.|0x09BF|
|Spark|0x0D15|
|Spark Technology Labs Inc.|0x0667|
|Sparkage Inc.|0x077E|
|SpartanLync Technologies Corp.<br>Bluetooth SIG Proprietary|0x10B5<br>P|



Page 421 of 442 

**Assigned Numbers** / Document 

**2026-06-23** 

|Spartek Systems Inc.|0x0D12|
|---|---|
|Spaulding Clinical Research|0x0593|
|SPD Development Company Ltd|0x03E1|
|Specifi-Kali LLC|0x0502|
|Spectra Precision (Kaiserslautern) GmbH|0x10DC|
|Spectrum Brands, Inc.|0x0356|
|Spectrum Technologies, Inc.|0x09F4|
|Sperry Labs LLC|0x0F74|
|spheretek japan|0x0FA7|
|Sphinx Electronics GmbH & Co KG|0x0454|
|SPIA Cycling Inc.|0x01BA|
|SPICA SYSTEMS LLC|0x0816|
|Spicebox LLC|0x0168|
|Spinlock Ltd|0x06E3|
|Spintly, Inc.|0x0AA9|
|SportIQ|0x012B|
|Spreadtrum Communications Shanghai Ltd|0x01EC|
|SPRiNTUS GmbH|0x0E83|
|SPX Aids to Navigation Oy|0x0F97|
|SQL Technologies Corp.|0x0A54|
|Square Panda, Inc.|0x0625|
|Square, Inc.|0x0AEB|
|SR-Medizinelektronik|0x00A1|
|SRAM|0x0933|
|SSV Software Systems GmbH|0x0AD3|
|ST Microelectronics|0x0030|
|St. Jude Medical, Inc.|0x03C5|
|STABILO International|0x04E8|
|Staccato Communications, Inc.|0x004D|
|Stalmart Technology Limited|0x00BF|
|Stamer Musikanlagen GMBH|0x022A|
|Stanley Black and Decker|0x00FE|
|stAPPtronics GmbH|0x05E2|
|Star Micronics Co., Ltd.|0x01B0|
|Star Technologies|0x0589|
|Stark Future SL|0x0FD0|



Bluetooth SIG Proprietary 

Page 422 of 442 

**Assigned Numbers** / Document 

**2026-06-23** 

|Starkey Hearing Technologies|0x00BA|
|---|---|
|StarLeaf Ltd|0x0796|
|STARLITE Co., Ltd.|0x07DA|
|START TODAY CO.,LTD.|0x058A|
|Stasis Labs, Inc.|0x0651|
|Statsports International|0x04C0|
|Status Audio LLC|0x0D68|
|Steelcase, Inc.|0x0473|
|Steelseries ApS|0x0111|
|Steinel GmbH|0x0563|
|Steinel Solutions AG|0x09EF|
|Steiner-Optik GmbH|0x04CA|
|Stemco Products Inc|0x0697|
|STEMP Inc.|0x0207|
|Step One Limited|0x099E|
|StepUp Solutions ApS|0x0C5D|
|steute Schaltgerate GmbH & Co. KG|0x0279|
|STEYR Sport GmbH|0x0EAB|
|Stinger Equipment, Inc.|0x0C3F|
|STL|0x0AB9|
|Stogger B.V.|0x0D5D|
|StoneDevices|0x0FB4|
|StoneL|0x0281|
|Stonestreet One, LLC|0x005E|
|STOREIO TECHNOLOGIES LTD|0x0F89|
|storm power ltd|0x0297|
|Storz & Bickel GmbH & Co. KG|0x06C8|
|Strainstall Ltd|0x019F|
|Streamit B.V.|0x0AD5|
|StreetCar ORV, LLC|0x0C72|
|Structural Health Systems, Inc.|0x03C7|
|STRYDE LIMITED|0x106E|
|stryker|0x0A5F|
|SUBARU Corporation|0x0A10|
|Subeca, Inc.|0x0851|
|Successful Endeavours Pty Ltd<br>Bluetooth SIG Proprietary|0x076F<br>P|



Page 423 of 442 

**Assigned Numbers** / Document 

**2026-06-23** 

|Summit Data Communications, Inc.|0x006E|
|---|---|
|SUNCORPORATION|0x0904|
|Sunplus Technology Co., Ltd.|0x0AE7|
|Sunrise Micro Devices, Inc.|0x01AF|
|Sunstone-RTLS Ipari Szolgaltato Korlatolt Felelossegu Tarsasag|0x0C9C|
|Super B Lithium Power B.V.|0x0BD5|
|Surefire, LLC|0x0308|
|SUREPULSE MEDICAL LIMITED|0x0E05|
|Suunto Oy|0x009F|
|Suzhou Fanxi Technology Co., Ltd.|0x0FAA|
|Suzhou Pairlink Network Technology|0x05B9|
|Svantek Sp. z o.o.|0x04C7|
|Svep Design Center AB|0x0452|
|SwaraLink Technologies|0x0729|
|Swedlock AB|0x0AC9|
|Swift IOT Tech (Shenzhen) Co., LTD.|0x0E3B|
|Swiftronix AB|0x04B5|
|SwingLync L. L. C.|0x04FC|
|SwipeSense, Inc.|0x0CB0|
|Swipp ApS|0x0305|
|Swirl Networks|0x00B5|
|Swiss Audio SA|0x0407|
|SWISSINNO SOLUTIONS AG|0x0BBB|
|Swissprime Technologies AG|0x02BA|
|Sylero|0x0551|
|Sylvac sa|0x0BCC|
|Symbol Technologies, Inc.|0x002A|
|Synapse Electronics|0x0284|
|Synaptics Incorporated|0x0A76|
|SYNCHRON, INC.|0x0D31|
|Synergy Tecnologia em Sistemas Ltda|0x09D8|
|Syng Inc|0x0A15|
|Synkopi, Inc.|0x0D1F|
|Synopsys, Inc.|0x0031|
|Syntronix Corporation|0x0038|
|SYSDEV Srl|0x03CB|



Bluetooth SIG Proprietary 

Page 424 of 442 

**Assigned Numbers** / Document 

**2026-06-23** 

|System Elite Holdings Group Limited|0x0D25|
|---|---|
|SYSTEM LOCO LTD|0x1083|
|Systemic Games, LLC|0x0D39|
|Systems and Chips, Inc|0x003E|
|Syszone Co., Ltd|0x01BD|
|SZ Avinox Innovation Co., Ltd.|0x10DB|
|SZ DJI TECHNOLOGY CO.,LTD|0x08AA|
|SZR-Dev UG|0x0E7F|
|T&A Laboratories LLC|0x0694|
|T&D|0x0392|
|T+A elektroakustik GmbH & Co.KG|0x0BFC|
|T-Mobile USA|0x0C8C|
|T2REALITY SOLUTIONS PRIVATE LIMITED|0x0BBC|
|T5 tek, Inc.|0x0BF5|
|TACHIKAWA CORPORATION|0x0D92|
|Taco, Inc.|0x0C53|
|Tactica Defense LLC|0x0F45|
|Tactile Engineering, Inc.|0x0C4E|
|Tactrix|0x0E94|
|Tacx b.v.|0x0689|
|Taelek Oy|0x048A|
|TAG HEUER SA|0x0CE7|
|Tag-N-Trac Inc|0x09E1|
|Taiga Motors Inc.|0x0B83|
|TaigaIoT|0x0FBE|
|Taiko Audio B.V.|0x0D7D|
|Tait International Limited|0x088F|
|Taiwan Fuhsing|0x0CE3|
|Taiwan Intelligent Home Corp.|0x08E9|
|Taixingbang Technology (HK) Co,. LTD.|0x00D3|
|Taiyo Yuden Co., Ltd|0x05E4|
|TAMADIC Co., Ltd.|0x0DE2|
|Tamblue Oy|0x0CF9|
|TAMRON Co., Ltd.|0x0EE3|
|Tandem Diabetes Care|0x059D|
|Tangerine, Inc.<br>Bluetooth SIG Proprietary|0x014E<br>P|



Page 425 of 442 

**Assigned Numbers** / Document 

**2026-06-23** 

|Tangshan HongJia electronic technology co., LTD.|0x0842|
|---|---|
|Taobao|0x01A8|
|Tap Sound System|0x0854|
|Tapkey GmbH|0x04FD|
|Target Corporation|0x0520|
|TASER International, Inc.|0x034D|
|TASKA PROSTHETICS LIMITED|0x04D4|
|taskit GmbH|0x017B|
|TATTCOM LLC|0x080E|
|tatwah SA|0x0818|
|TBS Electronics B.V.|0x05C9|
|TCL COMMUNICATION EQUIPMENT CO.,LTD.|0x0BC6|
|TDK Corporation|0x060D|
|TE Connectivity Corporation|0x08DE|
|TEAC Corporation|0x0C77|
|TeAM Hutchins AB|0x04C4|
|Tec4med LifeScience GmbH|0x0A13|
|TecBakery GmbH|0x0420|
|TECH FASS s.r.o.|0x10C3|
|Tech OVN Private Limited|0x0F07|
|Tech-Venom Entertainment Private Limited|0x0B29|
|Tech4home, Lda|0x0217|
|TECHFLITTER SOLUTIONS PRIVATE LIMITED|0x0F70|
|Technicolor USA Inc.|0x02AF|
|Technocon Engineering Ltd.|0x0C8E|
|Technogym SPA|0x026D|
|TECHNOLOGIES FOR FREERIDE s.r.o|0x0F31|
|Technology Solutions (UK) Ltd|0x01E6|
|Technosphere Labs Pvt. Ltd.|0x0918|
|TechSwipe|0x0CD3|
|TECHTICS ENGINEERING B.V.|0x0BC4|
|Techtronic Power Tools Technology Limited|0x02C3|
|Tedee Sp. z o.o.|0x0725|
|Teenage Engineering AB|0x0450|
|TEGAM, Inc.|0x05D5|
|TekHome|0x092B|



Bluetooth SIG Proprietary 

Page 426 of 442 

**Assigned Numbers** / Document 

**2026-06-23** 

|TEKTRO TECHNOLOGY CORPORATION|0x0D42|
|---|---|
|TEKZITEL PTY LTD|0x06A5|
|TELE System Communications Pte. Ltd.|0x0DB1|
|Tele-Radio i Lysekil AB|0x0E8A|
|Telecom Design|0x0B96|
|Telecon Mobile Limited|0x05A6|
|Teledyne Instruments, Inc.|0x0EFE|
|Teledyne Lecroy, Inc.|0x059A|
|Telemacy Ltd|0x0FD5|
|Telemonitor, Inc.|0x017A|
|Telenor ASA|0x0991|
|Telink Semiconductor Co. Ltd|0x0211|
|Telit Wireless Solutions GmbH|0x008F|
|TEMEC Instruments B.V.|0x012C|
|TEMKIN ASSOCIATES, LLC|0x0A9A|
|Temperature Sensitive Solutions Systems Sweden AB|0x09D2|
|Tempur World, LLC|0x10CF|
|Tencent Holdings Ltd.|0x013A|
|Tendyron Corporation|0x02A5|
|Tenovis|0x002B|
|Tensoli GmbH|0x109B|
|Tentacle Sync GmbH|0x043F|
|Terasite Technologies|0x0FAF|
|TeraTron GmbH|0x0984|
|TerOpta Ltd|0x0762|
|Tertium Technology|0x08DB|
|terumo|0x109D|
|TESA SA|0x053D|
|Tesla, Inc.|0x022B|
|Testo SE & Co. KGaA|0x0D84|
|TETNET|0x0F33|
|Teva Branded Pharmaceutical Products R&D, Inc.|0x02CE|
|Texas Instruments Inc.|0x000D|
|TGM TECHNOLOGY CO., LTD.|0x08CC|
|TGR 1.618 Limited|0x08A7|
|Thales Simulation & Training AG<br>Bluetooth SIG Proprietary|0x0D46<br>P|



Page 427 of 442 

**Assigned Numbers** / Document 

**2026-06-23** 

|Thalmic Labs Inc.|0x0562|
|---|---|
|The AI Toy Company|0x0FAD|
|The Apache Software Foundation|0x0B65|
|The Chamberlain Group, Inc.|0x0878|
|The Coca-Cola Company|0x0794|
|THE EELECTRIC MACARON LLC|0x0CB6|
|The Energy Conservatory, Inc.|0x067A|
|The Goodyear Tire & Rubber Company|0x0B99|
|The Idea Cave, LLC|0x02F1|
|The Indoor Lab, LLC|0x063E|
|The Kroger Co.|0x07AA|
|The L.S. Starrett Company|0x08F1|
|The Linux Foundation|0x05F1|
|The Shadow on the Moon|0x04B2|
|The University of Tokyo|0x01F3|
|The Wand Company Ltd|0x0A47|
|The Wildflower Foundation|0x078A|
|The90, Inc.|0x10CB|
|Theben AG|0x04F1|
|Thermo Fisher Scientific|0x0315|
|Thermokon-Sensortechnik GmbH|0x0C35|
|Thermomedics, Inc.|0x043E|
|ThermoWorks, Inc.|0x0B11|
|THERMY LTD|0x0EBA|
|Thetatronics Ltd|0x0398|
|ThingCo Limited|0x0F2F|
|ThingOS GmbH & Co KG|0x07DC|
|Thingsquare AB|0x0391|
|ThinkOptics, Inc.|0x0092|
|Thirdwayv Inc.|0x0772|
|Thomas Dynamics, LLC|0x07B1|
|Thorley Industries, LLC|0x097C|
|Thornwave Labs Inc|0x04C9|
|THOTAKA TEKHNOLOGIES INDIA PRIVATE LIMITED|0x0CF0|
|Thule Group AB|0x0CFE|
|Ticto N.V.|0x042C|



Bluetooth SIG Proprietary 

Page 428 of 442 

**Assigned Numbers** / Document 

**2026-06-23** 

|TIGER CORPORATION|0x0C39|
|---|---|
|TigerLight, Inc.|0x0E85|
|Tile, Inc.|0x067C|
|Time Location Systems AS|0x0E1B|
|Timebirds Australia Pty Ltd|0x0D0F|
|TimeKeeping Systems, Inc.|0x0083|
|Timer Cap Co.|0x015F|
|Timex Group USA, Inc.|0x00D6|
|Tiny Displays Inc.|0x108C|
|TireCheck GmbH|0x0BA2|
|TITUM AUDIO, INC.|0x0DAE|
|TiVo Corp|0x0471|
|Tivoli Audio, LLC|0x014A|
|TKH Security B.V.|0x0B20|
|tkLABS INC.|0x094D|
|TLV Co.,LTD.|0x1070|
|TM-TECHNOLOGIES, JSC|0x1075|
|ToGetHome Inc.|0x0408|
|TOITU CO., LTD.|0x09FB|
|TOKAI-DENSHI INC|0x0B58|
|Tokai-rika co.,ltd.|0x08BB|
|Token Zero Ltd|0x03A4|
|Tokenize, Inc.|0x083D|
|Tom Allebrandi Consulting|0x05A8|
|Tom Communication Industrial Co.,Ltd.|0x06FC|
|TOMOE VALVE CO.,LTD|0x1060|
|TomTom International BV|0x0100|
|Tongfang Health Technology (Beijing) Co., Ltd.|0x0EBD|
|tonies GmbH|0x0C8D|
|Toor Technologies LLC|0x057C|
|Topcon Positioning Systems, LLC|0x008B|
|TOPPAN FORMS CO.,LTD.|0x0354|
|Topre Corporation|0x08AC|
|TOR.AI LIMITED|0x0E72|
|Torrox GmbH & Co KG|0x0269|
|Toshiba Corp.<br>Bluetooth SIG Proprietary|0x0004<br>P|



Page 429 of 442 

**Assigned Numbers** / Document 

**2026-06-23** 

|TOTO LTD.|0x0A8F|
|---|---|
|TouchTronics, Inc.|0x0AB2|
|Touché Technology Ltd|0x06F4|
|ToughBuilt Industries LLC|0x0B12|
|TourBuilt, LLC|0x0A59|
|Toyo Electronics Corporation|0x05D9|
|TOYOTA motor corporation|0x0977|
|Toytec Corporation|0x0A4E|
|Tozoa LLC|0x0DDD|
|TPV Technology Limited|0x033D|
|TQ-Systems GmbH|0x0FB6|
|TRACERCO LIMITED|0x0F00|
|Trackonomy Systems, Inc.|0x0EF7|
|TRACKTING S.R.L.|0x0C67|
|Trackunit A/S|0x086F|
|TRACMO, INC.|0x05F7|
|Tractian Technologies Inc.|0x10C9|
|Trade FIDES a.s.|0x0681|
|Trainesense Ltd.|0x0424|
|Trakm8 Ltd|0x0238|
|Tramex Limited|0x05AA|
|Transducers Direct, LLC|0x010C|
|Transenergooil AG|0x01C2|
|Transilica, Inc.|0x0018|
|TRANSSION HOLDINGS LIMITED|0x0942|
|Trapper Data AB|0x06F2|
|TraqFreq LLC|0x0700|
|Travelxp India Private Limited|0x0E9E|
|Treegreen Limited|0x0874|
|Trek Bicycle|0x0B86|
|TreLab Ltd|0x00B7|
|Trezor Company s.r.o.|0x0F29|
|Triax Technologies Inc|0x060B|
|Tricorder Arraay Technologies LLC|0x088B|
|Tridentify AB|0x0FBF|
|Triductor Technology (Suzhou), Inc.<br>Bluetooth SIG Proprietary|0x0C7A<br>P|



Page 430 of 442 

**Assigned Numbers** / Document 

**2026-06-23** 

|TrikThom|0x0CC8|
|---|---|
|Trimble Inc.|0x03FD|
|Trineo Sp. z o.o.|0x01B4|
|Triple W Japan Inc.|0x0B75|
|Triumph Designs Ltd|0x10DD|
|Trivedi Advanced Technologies LLC|0x08B0|
|Trividia Health, Inc.|0x01AC|
|TRON Forum|0x019A|
|Trotec GmbH|0x0DCC|
|TROX GmbH|0x0CBC|
|TRSystems GmbH|0x050D|
|TRUE Fitness Technology|0x03E7|
|True Wearables, Inc.|0x0577|
|Trulli Audio|0x0809|
|Truma Gerätetechnik GmbH & Co. KG|0x0C73|
|Try and E CO.,LTD.|0x05F2|
|TSC Auto-ID Technology Co., Ltd.|0x0902|
|TSE BRAKES, INC.|0x0987|
|TSI|0x0B42|
|TTPCom Limited|0x001A|
|TTS Tooltechnic Systems AG & Co. KG|0x044F|
|Tucker International LLC|0x0443|
|Tunstall Nordic AB|0x0451|
|Tussock Innovation 2013 Limited|0x06B2|
|TVS Motor Company Ltd.|0x0CF7|
|Twenty Five Seven, prodaja in storitve, d.o.o.|0x0BEA|
|TWINKLY SRL|0x0BDC|
|Twocanoes Labs, LLC|0x0228|
|twopounds gmbh|0x0C49|
|TWS Srl|0x06BC|
|txtr GmbH|0x00DA|
|TYKEE PTY. LTD.|0x0C16|
|Tymphany HK Ltd|0x0E84|
|Tymtix Technologies Private Limited|0x08F5|
|Typo Products, LLC|0x00FF|
|TYRI Sweden AB|0x0806|



Bluetooth SIG Proprietary 

Page 431 of 442 

**Assigned Numbers** / Document 

**2026-06-23** 

|Tyromotion GmbH|0x0CFB|
|---|---|
|Tyto Life LLC|0x0519|
|Tzero Technologies, Inc.|0x0051|
|U-Shin Ltd.|0x08B9|
|Ubiquitous Computing Technology Corporation|0x0105|
|ubisys technologies GmbH|0x08BE|
|Ugreen Group Limited|0x0FA6|
|Uhlmann & Zacher GmbH|0x049D|
|UKC Technosolution|0x0252|
|ULC Robotics Inc.|0x05BD|
|Ultimea Technology (Shenzhen) Limited|0x0D8C|
|UltronSMART Inc.|0x10D6|
|Ultune Technologies|0x0648|
|umanSense AB|0x0758|
|Undagrid B.V.|0x04AC|
|Under Armour|0x04E3|
|UNEEG medical A/S|0x0C57|
|Unfolded Circle ApS|0x0EAF|
|UNI-ELECTRONICS, INC.|0x0510|
|Unico RBC|0x0188|
|Unify Software and Solutions GmbH & Co. KG|0x0425|
|Unikey Technologies, Inc.|0x015E|
|UniqAir Oy|0x0A7B|
|Unisto AG|0x0AF9|
|Unitech Electronic Inc.|0x0AF5|
|Univations Limited|0x061C|
|Universal Audio, Inc.|0x09C2|
|Universal Biosensors Pty Ltd|0x0A81|
|Universal Electronics, Inc.|0x0093|
|UNIVERSAL REMOTE CONTROL, INC.|0x10BE|
|Universidad Politecnica de Madrid|0x0D35|
|University of Applied Sciences Valais/Haute Ecole Valaisanne|0x025A|
|University of Michigan|0x02E0|
|Unlimited Engineering SL|0x07DE|
|UnlimitedIRL LLC|0x1051|
|UnSeen Technologies Oy<br>Bluetooth SIG Proprietary|0x0659<br>P|



Page 432 of 442 

**Assigned Numbers** / Document 

**2026-06-23** 

|unu GmbH|0x0AE5|
|---|---|
|Unwire|0x02E6|
|UpRight Technologies LTD|0x0735|
|Urban Armor Gear, LLC|0x0E58|
|Urban Compass, Inc|0x06C5|
|Urbanista AB|0x0AA7|
|Urbanminded Ltd|0x0805|
|User Hello, LLC|0x0907|
|USound GmbH|0x0846|
|UTC Fire and Security|0x01F4|
|UVISIO|0x09C4|
|Uwatec AG|0x024A|
|V-ZUG Ltd|0x05CE|
|Valencell, Inc.|0x0440|
|ValenceTech Limited|0x00FD|
|VALEO MANAGEMENT SERVICES|0x0C48|
|Valeo Service|0x01EE|
|Valostra Digital Ventures LLP|0x0F91|
|Valtech|0x0877|
|Valve Corporation|0x055D|
|VANBOX|0x0E3E|
|VANMOOF Global Holding B.V.|0x0A4F|
|Variowell Development GmbH|0x10BD|
|VC Inc.|0x090F|
|VCS Vision Control Solutions GmbH|0x1061|
|Vectronix AG|0x048C|
|VEGA Grieshaber KG|0x044D|
|VELCO|0x0D82|
|Velentium, LLC|0x0AEE|
|velocitux|0x0D27|
|VELUX A/S|0x06E7|
|Vemcon GmbH|0x0D48|
|VENGIT Korlatolt Felelossegu Tarsasag|0x0198|
|Vensi, Inc.|0x02F4|
|Venso EcoSolutions AB|0x066D|
|Veo Technologies ApS<br>Bluetooth SIG Proprietary|0x0DBA<br>P|



Page 433 of 442 

**Assigned Numbers** / Document 

**2026-06-23** 

|VERGESENSE INC|0x10BB|
|---|---|
|Verifone Systems Pte Ltd. Taiwan Branch|0x0200|
|verisilicon|0x05B5|
|Vermis, software solutions llc|0x0E34|
|Vernier Software & Technology|0x0152|
|VersaMe|0x0349|
|Vertex International, Inc.|0x0405|
|Vertu Corporation Limited|0x00A2|
|Verve InfoTec Pty Ltd|0x0CA7|
|Vervent Audio Group|0x0BA4|
|Vessel Ltd.|0x09BE|
|vhf elektronik GmbH|0x0461|
|Viaanix, Inc.|0x0E95|
|Viasat Group S.p.A.|0x0591|
|Vibe Energy B.V.|0x0F5A|
|VIBRADORM GmbH|0x03B0|
|Vibrissa Inc.|0x050B|
|ViCentra B.V.|0x023D|
|Victor Hasselblad AB|0x10D4|
|Victron Energy BV|0x02E1|
|Vigil Technologies Inc.|0x06F5|
|VIMANA TECH PTY LTD|0x0B85|
|Vimar SpA|0x0760|
|Vinetech Co., Ltd|0x04A6|
|VINYL MATT MEDIA LIMITED|0x0ED1|
|Viper Design LLC|0x05E0|
|Vire Health Oy|0x0C99|
|Virscient Limited|0x08D2|
|VIRTUALCLINIC.DIRECT LIMITED|0x05DF|
|Viselabs|0x0E3C|
|Vision Group Inc.|0x0F2C|
|Visiontronic s.r.o.|0x0AB0|
|Visteon Corporation|0x00A7|
|Visuallex Sport International Limited|0x0ACD|
|Visybl Inc.|0x0112|
|VIT Initiative, LLC<br>Bluetooth SIG Proprietary|0x04BF<br>P|



Page 434 of 442 

**Assigned Numbers** / Document 

**2026-06-23** 

|Vitio Medical S.L.|0x0F3D|
|---|---|
|Vitulo Plus BV|0x06F6|
|Vivago Oy|0x0E7A|
|Vivint, Inc.|0x0DE8|
|VIVIWARE JAPAN, Inc.|0x0D79|
|vivo Mobile Communication Co., Ltd.|0x0837|
|VivoSensMedical GmbH|0x09D9|
|Vizio, Inc.|0x0058|
|VIZPIN INC.|0x056E|
|Vocera Communications, Inc.|0x0ADE|
|VODALOGIC PTY LTD|0x0CE0|
|Vogels Products B.V.|0x0D3F|
|Volan Technology Inc.|0x0A0A|
|Volantic AB|0x016E|
|Volkswagen AG|0x011F|
|Vonkil Technologies Ltd|0x03CC|
|Vorwerk Elektrowerke GmbH & Co. KG|0x086E|
|VOS Systems, LLC|0x09A1|
|Vossloh-Schwabe Deutschland GmbH|0x062D|
|VOTRONIC Elektronik-Systeme GmbH|0x1045|
|Voxx International|0x0765|
|Voyetra Turtle Beach|0x00D9|
|VSC Synapse LLC|0x10B0|
|VSN Technologies, Inc.|0x00F7|
|VT42 Pty Ltd|0x0994|
|Vtrack Systems|0x00E9|
|VusionGroup|0x0A89|
|Vuzix Corporation|0x060C|
|Vyassoft Technologies Inc|0x03FA|
|Wabilogic Ltd.|0x06B5|
|Wacker Neuson SE|0x0BF7|
|WAFERLOCK|0x0A7C|
|Wahoo Fitness, LLC|0x01FC|
|WAKO CO,.LTD|0x0C65|
|Wally Ventures S.L.|0x0287|
|Walmart Inc.|0x0E3D|



Bluetooth SIG Proprietary 

Page 435 of 442 

**Assigned Numbers** / Document 

**2026-06-23** 

|Walt Disney|0x0183|
|---|---|
|Walter Mueller Inc. for Industrial Electronics|0x0F79|
|Wangi Lai PLT|0x0633|
|Wangs Alliance Corporation|0x0817|
|Wanzl GmbH & Co. KGaA|0x0E11|
|Warehouse Innovations|0x0130|
|WARES|0x08D8|
|Warner Bros.|0x0A6F|
|Watchdog Systems LLC|0x0C83|
|WatchGas B.V.|0x09F0|
|Water-i.d. GmbH|0x0797|
|WaterGuru, Inc.|0x041C|
|Watteam Ltd|0x03D0|
|WATTER, Inc.|0x0F63|
|WATTS ELECTRONICS|0x05CC|
|WavePlus Technology Co., Ltd.|0x0023|
|WaveRF, Corp.|0x0DFF|
|WaveWare Technologies Inc.|0x023F|
|Waybeyond Limited|0x07E6|
|Wazombi Labs OÜ|0x0370|
|WBS PROJECT H PTY LTD|0x0B19|
|Wearable Link Limited|0x0948|
|WearNex Limited|0x0EB1|
|WeatherFlow, Inc.|0x02AE|
|Webasto SE|0x0A79|
|Weber Sensors, LLC|0x0AFD|
|Weber-Stephen Products LLC|0x0DF2|
|WEG S.A.|0x0627|
|Wellang.Co,.Ltd|0x0D56|
|Wellington Drive Technologies Ltd|0x0578|
|Wellinks Inc.|0x0361|
|Wellnomics Ltd|0x05FF|
|Weltek Technologies Company Limited|0x0C87|
|WePower Technologies LLC|0x0F9C|
|Wernher von Braun Center for ASdvanced Research|0x0706|
|WESCO AG|0x0C90|



Bluetooth SIG Proprietary 

Page 436 of 442 

**Assigned Numbers** / Document 

**2026-06-23** 

|WEST inx Ltd.|0x0EC0|
|---|---|
|West Pharmaceutical Services, Inc.|0x07CB|
|Western Digital Techologies, Inc.|0x0867|
|Westfalen Aktiengesellschaft|0x10D8|
|WHERE, Inc.|0x03F5|
|Whirl Inc|0x0344|
|Whirlpool|0x0DB2|
|White Eagle Sonic Technologies, Inc.|0x0F67|
|White Horse Scientific ltd|0x05B4|
|Wicentric, Inc.|0x005F|
|Widcomm, Inc.|0x0011|
|Widex A/S|0x01E0|
|WIKA Alexander Wiegand SE & Co.KG|0x0989|
|Wildlife Acoustics, Inc.|0x0730|
|Wiliot LTD.|0x0500|
|WILKA Schliesstechnik GmbH|0x06F7|
|Wille Engineering|0x01A6|
|Williams Sound, LLC|0x105D|
|Willow Laboratories, Inc.|0x0EEC|
|Wilo SE|0x018C|
|Wilson Sporting Goods|0x02C4|
|Wimate Technology Solutions Pvt Ltd|0x0FCD|
|Wimoto Technologies Inc|0x0117|
|WindowMaster A/S|0x03F2|
|WINKEY ENTERPRISE (HONG KONG) LIMITED|0x0BDF|
|Wintersteiger AG|0x0883|
|WIOsense GmbH & Co. KG|0x0868|
|Wireless Cables Inc|0x0413|
|WirelessWERX|0x013D|
|Wiser Devices, LLC|0x0E3F|
|WiSilica Inc.|0x0197|
|WISYCOM S.R.L.|0x0B38|
|Withings|0x03FF|
|Witron Technology Limited|0x00F1|
|Witschi Electronic Ltd|0x089F|
|WIZNOVA, Inc.<br>Bluetooth SIG Proprietary|0x0745<br>P|



Page 437 of 442 

**Assigned Numbers** / Document 

**2026-06-23** 

|WKD Labs Ltd|0x0E40|
|---|---|
|WMF AG|0x0A33|
|Woan Technology (Shenzhen) Co., Ltd.|0x0969|
|Wolf Steel ltd|0x0CDA|
|Woncan (Hong Kong) Limited|0x0DF0|
|Wood IT Security, LLC|0x0698|
|Woodenshark|0x04E4|
|Woosim Systems Inc.|0x04FE|
|Workaround Gmbh|0x0B07|
|World Moto Inc.|0x0300|
|Worthcloud Technology Co.,Ltd|0x0A29|
|WOWTech Canada Ltd.|0x0285|
|WRLDS Creations AB|0x07C8|
|Wrmth Corp.|0x0C3D|
|WTO Werkzeug-Einrichtungen GmbH|0x0666|
|Wuhu Hongjing Electronic Co.,Ltd|0x0E5B|
|Wuhu Mengbo Technology Co., Ltd.|0x0E01|
|WuQi technologies, Inc.|0x076E|
|Wurth Elektronik eiSos GmbH & Co. KG|0x031A|
|Wuxi Does IOT Co., Ltd|0x0EF1|
|Wuxi Linkpower Microelectronics Co.,Ltd|0x0C5F|
|WuXi Vimicro|0x0081|
|WUXI WEIDA INTELLIGENT ELECTRONICS CO.,LTD.|0x0F5E|
|WWZN Information Technology Company Limited|0x0764|
|Wyler AG|0x0213|
|Wynd Technologies, Inc.|0x03CD|
|Wyze Labs, Inc|0x0870|
|Wyzelink Systems Inc.|0x03D5|
|x-Senso Solutions Kft|0x0232|
|XANTHIO|0x0889|
|Xenoma Inc.|0x093C|
|Xenter, Inc.|0x0AB6|
|Xi’an Fengyu Information Technology Co., Ltd.|0x0A9C|
|Xiamen Eholder Electronics Co.Ltd|0x0722|
|Xiamen Everesports Goods Co., Ltd|0x0567|
|Xiamen Intretech Inc.|0x0D7A|



Bluetooth SIG Proprietary 

Page 438 of 442 

**Assigned Numbers** / Document 

**2026-06-23** 

|Xiamen Mage Information Technology Co., Ltd.|0x07A4|
|---|---|
|Xiamen RUI YI Da Electronic Technology Co.,Ltd|0x0E16|
|Xian Yisuobao Electronic Technology Co., Ltd.|0x0C32|
|Xiant Technologies, Inc.|0x0DD7|
|Xiaomi Inc.|0x038F|
|Xicato Inc.|0x0253|
|XIHAO INTELLIGENGT TECHNOLOGY CO., LTD|0x0D36|
|XiQ|0x03AD|
|Xirgo Technologies, LLC|0x0BB5|
|XMI Systems SA|0x0267|
|Xradio Technology Co.,Ltd.|0x063D|
|XSENSE LTD|0x0CA2|
|XTel Wireless ApS|0x017F|
|Xtrava Inc.|0x044E|
|XUNTONG|0x09C8|
|Yale|0x0BDE|
|Yamaha Corporation|0x0A2A|
|YAMAHA MOTOR CO.,LTD.|0x0CA1|
|Yamaha Motor eBike Systems GmbH|0x1047|
|Yandex Services AG|0x0905|
|YanFeng Visteon(Chongqing) Automotive Electronic Co.,Ltd|0x0CCD|
|YANMAR HOLDINGS CO., LTD.|0x105C|
|Yardarm Technologies|0x025F|
|Yashu Systems|0x0C91|
|YDIIT Co., Ltd|0x0FD3|
|Yealink (Xiamen) Network Technology Co.,LTD|0x0850|
|Yeasound (Xiamen) Hearing Technology Co., Ltd|0x0D9A|
|Yellowcog|0x0507|
|Yichip Microelectronics (Hangzhou) Co.,Ltd.|0x050E|
|yikes|0x0161|
|YKK AP Inc.|0x099D|
|Yo-tronics Technology Co., Ltd.|0x0863|
|YOKOWO CO., LTD.|0x02BB|
|Yolni Inc.|0x0F1E|
|Yota Devices LTD|0x03D6|
|Yoto Limited|0x104B|



Bluetooth SIG Proprietary 

Page 439 of 442 

**Assigned Numbers** / Document 

**2026-06-23** 

|Ypsomed AG|0x0CDC|
|---|---|
|Yuanfeng Technology Co., Ltd.|0x0E0C|
|Yukai Engineering Inc.|0x0925|
|Yukon advanced optics worldwide, UAB|0x09EC|
|yupiteru|0x0C74|
|Yuquan Semiconductor (Xiamen) Co., Ltd.|0x0F23|
|Z-ONE Technology Co., Ltd.|0x0E42|
|ZanCompute Inc.|0x04F4|
|Zebra Technologies Corporation|0x01F1|
|Zeevo, Inc.|0x0012|
|Zeku Technology (Shanghai) Corp., Ltd.|0x0C2C|
|Zen-Me Labs Ltd|0x03D8|
|Zencontrol Pty Ltd|0x0AA8|
|Zerene Inc.|0x0D37|
|zero1.tv GmbH|0x0098|
|ZF OPENMATICS s.r.o.|0x040A|
|Zhejiang Desman Intelligent Technology Co., Ltd.|0x0E50|
|Zhejiang Huanfu Technology Co., LTD|0x0D97|
|Zhejiang Wanyou Intelligent Technology Co., Ltd.|0x10A8|
|Zhong Shan City Richsound Electronic Industrial Ltd.|0x0ECB|
|ZhuHai AdvanPro Technology Company Limited|0x0656|
|Zhuhai Hoksi Technology CO.,LTD|0x090A|
|Zhuhai Jieli technology Co.,Ltd|0x05D6|
|Zhuhai Pantum Electronisc Co., Ltd|0x0AD4|
|Zhuhai Smartlink Technology Co., Ltd|0x0C79|
|ZifferEins GmbH & Co. KG|0x0978|
|ZIIP Inc|0x0B52|
|ZILLIOT TECHNOLOGIES PRIVATE LIMITED|0x0D34|
|ZIMI CORPORATION|0x08CE|
|Zimi Innovations Pty Ltd|0x07F1|
|ZIMMERMANN PV-Steel Group GmbH & Co. KG|0x0F1D|
|Zintouch B.V.|0x0C55|
|Zipcar|0x036C|
|Zirbel Bike GmbH|0x0FC9|
|Zliide Technologies ApS|0x06DA|
|Zmartfun Electronics, Inc.<br>Bluetooth SIG Proprietary|0x06E9<br>P|



Page 440 of 442 

**Assigned Numbers** / Document 

**2026-06-23** 

|Zomm, LLC|0x0074|
|---|---|
|Zorachka LTD|0x083E|
|ZRF, LLC|0x087A|
|Zscan Software|0x008D|
|ZTE Corporation|0x0958|
|Ztove ApS|0x066C|
|ZTR Control Systems LLC|0x0536|
|Zucchetti Axess|0x0EB3|
|Zuli|0x0195|
|Zume, Inc.|0x07A7|
|Zumtobel Group AG|0x064C|
|Zuuka Limited|0x105E|
|Zwift, Inc.|0x094A|
|ZWILLING J.A. Henckels Aktiengesellschaft|0x0BE5|



Bluetooth SIG Proprietary 

Page 441 of 442 

**Assigned Numbers** / Document 

**2026-06-23** 

## **<u>8 References</u>** 

- [1] 3D Synchronization Profile Version 1.0.3 or later. 

- [2] Audio/Video Control Transport Protocol Version 1.4 or later. 

- [3] Audio/Video Distribution Transport Protocol Version 1.3 or later. 

- [4] Bluetooth Core Specification Version 5.4 or later. 

- [5] Bluetooth Network Encapsulation Protocol Version 1.0 or later. 

- [6] Common ISDN Access Profile Version v1.0 or later. 

- [7] Environmental Sensing Service Version 1.0 or later. 

- [8] Extended Service Discovery Profile for UPnP Version 1.1.1 or later. 

- [9] Hardcopy Cable Replacement Profile Version v1.2 or later. 

- [10] Health Device Profile Version v1.1 or later. 

- [11] Human Interface Device Profile Version 1.1.1 or later. 

- [12] Internet Protocol Support Profile Version 1.0.0 or later. 

- [13] IrDA Interoperability Version v2.0 or later. 

- [14] Mesh Binary Large Object Transfer Model Version 1.0. 

- [15] Mesh Device Firmware Update Model Version 1.0. 

- [16] Mesh Model Version 1.0.1 or later. 

- [17] Mesh Profile Version 1.0.1 or later. 

- [18] Mesh Protocol Version 1.1. 

- [19] Object Transfer Service Version v1.0 or later. 

- [20] Personal Area Networking Profile Version v1.0 or later. 

- [21] RFCOMM Version v12 or later. 

- [22] Supplement to the Bluetooth Core Specification Version 10 or later. 

- [23] Telephony Control Protocol Version v1.1 or later. 

- [24] Unrestricted Digital Information Profile. 

- [25] User Data Service Version 1.1 or later. 

- [26] WAP Bearer Version v1.0 or later. 

Bluetooth SIG Proprietary 

Page 442 of 442