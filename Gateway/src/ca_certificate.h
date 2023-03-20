/*
 * Copyright (c) 2018 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef __CA_CERTIFICATE_H__
#define __CA_CERTIFICATE_H__

/* By default only certificates in DER format are supported. If you want to use
 * certificate in PEM format, you can enable support for it in Kconfig.
 */

/* GlobalSign Root CA - R1 for https://google.com */
static const unsigned char ca_certificate[] = {
#include "wiliot_till_june_2023.der.inc"
};

#endif /* __CA_CERTIFICATE_H__ */
