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

#ifndef DDESHELLMANAGER_H
#define DDESHELLMANAGER_H

#include <QObject>
#include <QHash>

#include "dtkwidget_global.h"

struct wl_display;
struct wl_registry;
struct dde_shell;
struct dde_shell_surface;

QT_BEGIN_NAMESPACE
class QWindow;
QT_END_NAMESPACE

DWIDGET_BEGIN_NAMESPACE

class DDdeShellManager : public QObject {
    Q_OBJECT

public:
    static DDdeShellManager* instance();
    bool isValid();
    void setNoTitleBar(QWindow* window, bool noTitleBar);
    void setWindowRadius(QWindow* window, int radius);

private:
    explicit DDdeShellManager(QObject* parent = nullptr);
    ~DDdeShellManager() override;

    bool ensureManager();
    void handleGlobal(wl_registry* registry, uint32_t name,
        const char* interface, uint32_t version);
    dde_shell_surface *shellSurfaceFor(QWindow* window);
    static void registry_global(void* data, wl_registry* registry,
        uint32_t name, const char* interface, uint32_t version);
    static void registry_global_remove(void* data, wl_registry* registry,
        uint32_t name);

    wl_display* m_display = nullptr;
    wl_registry* m_registry = nullptr;
    dde_shell* m_manager = nullptr;
    bool m_tried = false;
    QHash<QWindow* , dde_shell_surface *> m_surfaces;
};

DWIDGET_END_NAMESPACE

#endif  // DDESHELLMANAGER_H
