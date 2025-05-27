// SPDX-License-Identifier: BSD-2-Clause
// Copyright CM4all GmbH
// author: Max Kellermann <max.kellermann@ionos.com>

#pragma once

#include "Connection.hxx"
#include "event/net/TemplateServerSocket.hxx"

typedef TemplateServerSocket<SpawnConnection,
			     Instance &> SpawnListener;
