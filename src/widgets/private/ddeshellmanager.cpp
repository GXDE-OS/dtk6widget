/*
 * Copyright (C) 2026 CharOfString <markus_verify@126.com>
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 * ----------------------------------------------------------------------------
 * The solution is from GXDE's fork of Deepin-Menu
 */

#include <QGuiApplication>
#include <QWindow>
#include <QDebug>

#include <qpa/qplatformnativeinterface.h>
#include <wayland-client.h>

#include "dde-shell-client-protocol.h"
#include "ddeshellmanager.h"

DWIDGET_BEGIN_NAMESPACE

namespace {

bool shellDebug() {
    static const bool on = qEnvironmentVariableIsSet("DTK_WINDOW_DEBUG");
    return on;
}

void* nativeIntegrationResource(const QByteArray& name) {
    if (QPlatformNativeInterface* native_int =
            QGuiApplication::platformNativeInterface()) {
        return native_int->nativeResourceForIntegration(name);
    }

    return nullptr;
}

wl_surface* surfaceForWindow(QWindow* window) {
    if (!window) {
        return nullptr;
    }

    if (QPlatformNativeInterface* native_int =
            QGuiApplication::platformNativeInterface()) {
        return static_cast<wl_surface *>(native_int->nativeResourceForWindow(
            "surface", window));
    }
    return nullptr;
}

}  // namespace

DDdeShellManager* DDdeShellManager::instance() {
    static DDdeShellManager* self = new DDdeShellManager(qGuiApp);
    return self;
}

DDdeShellManager::DDdeShellManager(QObject* parent) : QObject(parent) {}

DDdeShellManager::~DDdeShellManager() {
    const auto surfaces = m_surfaces;
    for (dde_shell_surface* s : surfaces) {
        if (s) {
            dde_shell_surface_destroy(s);
        }
    }
    m_surfaces.clear();
}

void DDdeShellManager::registry_global(void* data, wl_registry* registry,
        uint32_t name, const char* interface, uint32_t version) {
    static_cast<DDdeShellManager *>(data)->handleGlobal(registry, name,
        interface, version);
}

void DDdeShellManager::registry_global_remove(void*, wl_registry*, uint32_t) {}

void DDdeShellManager::handleGlobal(wl_registry* registry, uint32_t name,
        const char* interface, uint32_t /*version*/) {
    if (qstrcmp(interface, "dde_shell") == 0) {
        m_manager = static_cast<dde_shell *>(
            wl_registry_bind(registry, name, &dde_shell_interface, 2));
    }
}

bool DDdeShellManager::ensureManager() {
    if (m_tried) {
        return m_manager != nullptr;
    }

    m_tried = true;

    m_display = static_cast<wl_display *>(nativeIntegrationResource("display"));
    if (!m_display) {
        return false;
    }

    m_registry = wl_display_get_registry(m_display);
    if (!m_registry) {
        return false;
    }

    static const wl_registry_listener listener = {
        registry_global,
        registry_global_remove,
    };

    wl_registry_add_listener(m_registry, &listener, this);
    wl_display_roundtrip(m_display);

    if (shellDebug())
        qDebug() << "(DWindow) Bound dde-shell:" << (m_manager != nullptr);

    return m_manager != nullptr;
}

bool DDdeShellManager::isValid() {
    return ensureManager() && m_manager != nullptr;
}

dde_shell_surface* DDdeShellManager::shellSurfaceFor(QWindow* window) {
    if (!window) {
        return nullptr;
    }

    if (auto it = m_surfaces.constFind(window); it != m_surfaces.constEnd()) {
        return it.value();
    }

    if (!ensureManager() || !m_manager) {
        return nullptr;
    }

    wl_surface* surface = surfaceForWindow(window);
    if (!surface) {
        return nullptr;
    }

    dde_shell_surface* ss = dde_shell_get_shell_surface(m_manager, surface);
    if (!ss) {
        return nullptr;
    }

    m_surfaces.insert(window, ss);
    connect(window, &QObject::destroyed, this, [this, window]() {
        if (dde_shell_surface* s = m_surfaces.take(window)) {
            dde_shell_surface_destroy(s);
        }
    });
    return ss;
}

void DDdeShellManager::setNoTitleBar(QWindow* window, bool noTitleBar) {
    dde_shell_surface* ss = shellSurfaceFor(window);
    if (!ss) {
        return;
    }

    // NoTitleBar: 1 = CSD
    wl_array arr;
    wl_array_init(&arr);
    if (int *p = static_cast<int *>(wl_array_add(&arr, sizeof(int)))) {
        *p = noTitleBar ? 1 : 0;
    }

    dde_shell_surface_set_property(ss, DDE_SHELL_PROPERTY_NOTITLEBAR, &arr);
    wl_array_release(&arr);
    wl_display_flush(m_display);

    if (shellDebug())
        qDebug() << "(DWindow) setNoTitleBar" << noTitleBar << "for" << window;
}

void DDdeShellManager::setWindowRadius(QWindow* window, int radius) {
    dde_shell_surface* ss = shellSurfaceFor(window);
    if (!ss) {
        return;
    }

    // WindowRadius is two flow, standand for radius for x and y separately
    wl_array arr;
    wl_array_init(&arr);
    if (float* p = static_cast<float *>(wl_array_add(&arr,
            sizeof(float) * 2))) {
        p[0] = static_cast<float>(radius);
        p[1] = static_cast<float>(radius);
    }
    dde_shell_surface_set_property(ss, DDE_SHELL_PROPERTY_WINDOWRADIUS, &arr);
    wl_array_release(&arr);
    wl_display_flush(m_display);

    if (shellDebug())
        qDebug() << "(DWindow) setWindowRadius" << radius << "for" << window;
}

DWIDGET_END_NAMESPACE
