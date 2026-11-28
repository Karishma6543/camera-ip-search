# Vendor protocol research: CP PLUS and Prama

Research snapshot: 2026-11-28. This document records public vendor/product evidence. It is not a certification statement and does not authorize use of proprietary SDKs.

## Decision

Use ONVIF as the first cross-brand integration. Identify each camera by the exact manufacturer, model, hardware revision and firmware reported by the device, then probe only the ONVIF operations that are implemented and safe. Keep CP PLUS and Prama adapters as thin extensions over the shared ONVIF adapter until vendor documentation demonstrates a required proprietary operation.

Do not infer that all models from a brand share the same ONVIF profile, authentication mode, endpoints or operation permissions.

## Evidence reviewed

| Vendor / model evidence | Publicly documented capability | Engineering implication | Evidence level |
|---|---|---|---|
| CP PLUS CP-UVR-0401F1V-I | ONVIF 24.12 Profiles T/S/G, CGI and SDK are listed on the vendor product page | ONVIF should be the generic path; CGI/SDK are possible future adapter paths, but no endpoint contract or redistributable SDK was established by this research | Vendor product-page claim |
| CP PLUS CP-UVR-0801E1V-I | ONVIF Profiles T/S/G, CGI and SDK listed | Same approach; verify exact firmware and device role (NVR vs camera) before offering camera-only operations | Vendor product-page claim |
| CP PLUS user-manual library | Vendor publishes model-specific manuals and a searchable catalogue | Use exact model manual before adding brand-specific configuration; do not generalize from an NVR manual to an IPC | Vendor documentation index |
| Prama PT-NC143D3-WNM(D2) | ONVIF Profiles S/T; product specification lists HTTP digest, WSSE and ONVIF digest authentication, TLS 1.3 | Supports prioritizing UsernameToken/ONVIF authentication interoperability tests; model/firmware still needs hardware verification | Vendor product-page claim |
| Prama PT-NC126D3-WNRAM(D2) | ONVIF Profiles S/G/T; product specification lists WSSE and ONVIF digest authentication | Same generic ONVIF path; verify each ONVIF operation and account role on hardware | Vendor product-page claim |
| Prama third-party-camera guide | Vendor describes ONVIF as the route for adding third-party cameras to its NVR | Supports ONVIF as the interoperability baseline, not a promise that every feature is exposed | Vendor guidance |


| CP PLUS official software catalogue | Lists **CPPLUS_IP Finder_Eng_Win V4.0** as a downloadable Orange-series utility; vendor product-documents page describes IP Finder as searching/configuring cameras on the local LAN | Confirms a vendor-supported local discovery/configuration utility exists. The public page does not expose its packet format, activation API, SDK contract or redistribution terms; do not imitate undocumented traffic | Official software catalogue |
| CP PLUS official Orange-series NVR manual | Describes searching uninitialized devices and a device-initialization workflow in the NVR camera-management interface | Confirms some CP PLUS product families expose a distinct initialization stage beyond normal ONVIF discovery. This NVR workflow is not proof that every standalone IPC exposes the same web page or protocol | Official model-family manual |
| Prama Online Pro official app guidance | Describes searching for local devices when camera and app are on the same network; otherwise adding by DeviceID | Confirms a Prama-supported local discovery workflow exists in its app. Public evidence reviewed does not disclose a desktop initialization protocol or an activation API | Official Prama app guidance |

## Public source links

- CP PLUS product page, CP-UVR-0401F1V-I: https://cpplusworld.com/cp-uvr-0401f1v-i-
- CP PLUS product page, CP-UVR-0801E1V-I: https://web.cpplusworld.com/cp-uvr-0801e1v-i
- CP PLUS official user-manual catalogue: https://cpplusworld.com/user-manual
- CP PLUS official product-document index (states IP Finder searches/configures cameras on the LAN): https://cpplusworld.com/productdocuments
- CP PLUS official software catalogue (lists CPPLUS_IP Finder_Eng_Win V4.0): https://cpplusworld.com/software
- CP PLUS Orange-series NVR manual (uninitialized-device search and initialization workflow): https://cpplusworld.com/orange-series/img/User_Manual_NVR.pdf
- Prama Online Pro official app description (local-network device search): https://www.pramaindia.in/prama-online-pro-app-security-camera-application/
- Prama PT-NC143D3-WNM(D2): https://www.pramaindia.in/product/pt-nc143d3-wnmd2/
- Prama PT-NC126D3-WNRAM(D2): https://www.pramaindia.in/product/pt-nc126d3-wnramd2/
- Prama guide to adding third-party cameras through ONVIF: https://www.pramaindia.in/how-to-add-a-third-party-camera-with-your-prama-nvr/

## Implementation boundaries

1. The generic ONVIF adapter must remain the default for all brands.
2. WS-Security UsernameToken PasswordDigest is implemented in the application, but interoperability with the above exact models is not yet hardware-tested.
3. Do not add guessed CGI paths, vendor login flows, default credentials, undocumented ports or reverse-engineered password-reset calls.
4. Before adding a proprietary adapter, obtain the model-specific API/SDK documentation, confirm redistribution/licensing terms, and build a simulator fixture from documented requests/responses.
5. Every write operation must remain single-device, user-confirmed and verified by a read-back where the protocol supports it.
6. Record actual results by exact model, hardware revision, firmware, account role and application commit. A successful ONVIF discovery is not proof that network writes, video streaming, events or PTZ work.

## Next validation request

For CP PLUS and Prama, request access to one representative camera of each intended product family, its exact model/firmware details, a test account with documented permissions, and the vendor's authorized SDK/API documentation if proprietary configuration is required. Never collect or commit camera passwords.
