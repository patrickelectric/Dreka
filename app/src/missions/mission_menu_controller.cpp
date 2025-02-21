#include "mission_menu_controller.h"

#include <QDebug>

#include "locator.h"
#include "mission_traits.h"

using namespace md::domain;
using namespace md::presentation;

MissionMenuController::MissionMenuController(QObject* parent) :
    QObject(parent),
    m_missionsService(md::app::Locator::get<IMissionsService>())
{
    connect(m_missionsService, &IMissionsService::missionRemoved, this, [this](Mission* mission) {
        if (m_mission != mission)
            return;

        m_mission = nullptr;
        m_missionItem = nullptr;
        emit routeItemChanged();
    });
}

bool MissionMenuController::canGoto() const
{
    return m_mission && m_missionItem && !m_missionItem->current();
}

QVariant MissionMenuController::route() const
{
    return m_mission ? m_mission->id() : QVariant();
}

int MissionMenuController::inRouteIndex() const
{
    if (!m_mission || !m_missionItem)
        return -1;

    return m_mission->route()->index(m_missionItem);
}

void MissionMenuController::invokeMenu(const QVariant& missionId, int index, double x, double y)
{
    m_mission = m_missionsService->mission(missionId);
    m_missionItem = m_mission ? m_mission->route()->item(index) : nullptr;

    emit routeItemChanged();

    if (m_missionItem)
        emit menuInvoked(x, y);
}

void MissionMenuController::drop()
{
    emit dropped();
}

void MissionMenuController::remove()
{
    if (!m_mission || !m_missionItem)
        return;

    m_mission->route()->removeItem(m_missionItem);
    m_missionsService->saveMission(m_mission);
}

void MissionMenuController::gotoItem()
{
    if (!m_missionItem)
        return;

    emit m_missionItem->goTo();
}
