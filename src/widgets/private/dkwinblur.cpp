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
 */

#include <QGuiApplication>
#include <QWindow>
#include <QDebug>

#include <qpa/qplatformnativeinterface.h>
#include <wayland-client.h>

#include "dkwinblur.h"
#include "blur-client-protocol.h"

DWIDGET_BEGIN_NAMESPACE

namespace {

bool menuDebug() {
    static const bool on = qEnvironmentVariableIsSet("DTK_MENU_DEBUG");
    return on;
}

uint32_t menuStrength() {
    bool ok = false;
    const uint32_t v = qEnvironmentVariableIntValue("DTK_MENU_BLUR_STRENGTH",
        &ok);
    return ok ? v : 300;
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

DKWinBlurManager* DKWinBlurManager::instance() {
    static DKWinBlurManager* self = new DKWinBlurManager(qGuiApp);
    return self;
}

DKWinBlurManager::DKWinBlurManager(QObject* parent) : QObject(parent) {}

DKWinBlurManager::~DKWinBlurManager() {
    const auto blurs = m_blurs;
    for (org_kde_kwin_blur* b : blurs) {
        if (b) {
            org_kde_kwin_blur_release(b);
        }
    }

    m_blurs.clear();
}

void DKWinBlurManager::registry_global(void* data, wl_registry* registry,
        uint32_t name, const char* interface, uint32_t version) {
    static_cast<DKWinBlurManager *>(data)->handleGlobal(registry, name,
        interface, version);
}

void DKWinBlurManager::registry_global_remove(void*, wl_registry*, uint32_t) {}

void DKWinBlurManager::handleGlobal(wl_registry* registry, uint32_t name,
        const char* interface, uint32_t /*version*/) {
    if (qstrcmp(interface, "org_kde_kwin_blur_manager") == 0) {
        m_manager = static_cast<org_kde_kwin_blur_manager *>(
            wl_registry_bind(registry, name,
                &org_kde_kwin_blur_manager_interface, 1));
    }
}

bool DKWinBlurManager::ensureManager() {
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

    if (menuDebug()) {
        qDebug() << "(DMenu) Blur MGR bound:" << (m_manager != nullptr);
    }

    return m_manager != nullptr;
}

bool DKWinBlurManager::isValid() {
    return ensureManager() && m_manager != nullptr;
}

void DKWinBlurManager::releaseBlur(QWindow* window) {
    if (org_kde_kwin_blur* b = m_blurs.take(window)) {
        org_kde_kwin_blur_release(b);
    }
}

void DKWinBlurManager::setBlur(QWindow* window, const QRegion& region) {
    if (!window || region.isEmpty()) {
        return;
    }

    if (!ensureManager() || !m_manager) {
        return;
    }

    wl_surface* surface = surfaceForWindow(window);
    wl_compositor* compositor =
        static_cast<wl_compositor *>(nativeIntegrationResource("compositor"));
    if (!surface || !compositor)
        return;

    releaseBlur(window);
    org_kde_kwin_blur* blur = org_kde_kwin_blur_manager_create(
        m_manager, surface);
    if (!blur) {
        return;
    }

    m_blurs.insert(window, blur);
    connect(window, &QObject::destroyed, this, [this, window]() {
        releaseBlur(window);
    });

    wl_region* reg = wl_compositor_create_region(compositor);
    for (const QRect& r : region)
        wl_region_add(reg, r.x(), r.y(), r.width(), r.height());
    org_kde_kwin_blur_set_region(blur, reg);

    const uint32_t strength = menuStrength();
    org_kde_kwin_blur_set_strength(blur, strength);
    org_kde_kwin_blur_commit(blur);
    wl_region_destroy(reg);

    wl_display_flush(m_display);

    if (menuDebug()) {
        static int callCount = 0;
        qDebug() << "(DMenu) Blur: #" << ++callCount
            << "; Region bounds:" << region.boundingRect()
            << "; Strength:" << strength;
    }
}

void DKWinBlurManager::clearBlur(QWindow* window) {
    if (!window) {
        return;
    }

    releaseBlur(window);
    if (m_display) {
        wl_display_flush(m_display);
    }
}

DWIDGET_END_NAMESPACE
