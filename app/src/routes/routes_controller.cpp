#include "routes_controller.h"

#include <QDebug>

#include "locator.h"
#include "route_traits.h"

using namespace md::domain;
using namespace md::presentation;

RoutesController::RoutesController(QObject* parent) :
    QObject(parent),
    m_routesRepository(md::app::Locator::get<IRoutesRepository>()),
    m_missionsRepository(md::app::Locator::get<IMissionsRepository>())
{
    Q_ASSERT(m_routesRepository);
    Q_ASSERT(m_missionsRepository);

    connect(m_routesRepository, &IRoutesRepository::routeTypesChanged, this,
            &RoutesController::routeTypesChanged);
    connect(m_routesRepository, &IRoutesRepository::routeAdded, this,
            &RoutesController::onRouteAdded);
    connect(m_routesRepository, &IRoutesRepository::routeRemoved, this,
            &RoutesController::onRouteRemoved);
    connect(m_routesRepository, &IRoutesRepository::routeChanged, this, [this](Route* route) {
        if (route == m_selectedRoute)
            emit selectedRouteChanged(route->id());
    });

    for (Route* route : m_routesRepository->routes())
    {
        this->onRouteAdded(route);
    }

    connect(m_missionsRepository, &IMissionsRepository::missionRemoved, this,
            [this](Mission* mission) {
                if (m_activeMission == mission)
                    this->setActiveMission(QVariant());
            });
}

QJsonArray RoutesController::routeTypes() const
{
    QJsonArray jsons;
    for (auto routeType : m_routesRepository->routeTypes())
    {
        jsons.append(QJsonObject::fromVariantMap(routeType->toVariantMap()));
    }
    return jsons;
}

QVariantList RoutesController::routeIds() const
{
    return m_routesRepository->routeIds();
}

QVariant RoutesController::selectedRoute() const
{
    return m_selectedRoute ? m_selectedRoute->id() : QVariant();
}

QJsonObject RoutesController::routeData(const QVariant& routeId) const
{
    Route* route = m_routesRepository->route(routeId);
    if (!route)
        return QJsonObject();

    QVariantMap routeData = route->toVariantMap();
    routeData[props::waypoints] = route->waypointsCount();
    routeData[props::type] = route->type()->name;

    return QJsonObject::fromVariantMap(routeData);
}

QJsonObject RoutesController::waypointData(const QVariant& routeId, int index) const
{
    Route* route = m_routesRepository->route(routeId);
    if (!route)
        return QJsonObject();

    RouteItem* waypoint = route->waypoint(index);
    if (!waypoint)
        return QJsonObject();

    return QJsonObject::fromVariantMap(waypoint->toVariantMap());
}

QJsonArray RoutesController::waypointTypes(const QVariant& routeId) const
{
    Route* route = m_routesRepository->route(routeId);
    if (!route)
        return QJsonArray();

    QJsonArray jsons;
    for (auto wptType : route->type()->waypointTypes)
    {
        jsons.append(QJsonObject::fromVariantMap(wptType->toVariantMap()));
    }
    return jsons;
}

void RoutesController::setActiveMission(const QVariant& missionId)
{
    m_activeMission = m_missionsRepository->mission(missionId);

    if (m_activeMission && m_activeMission->route() && !m_selectedRoute)
    {
        this->selectRoute(m_activeMission->route()->id());
    }
}

void RoutesController::selectRoute(const QVariant& selectedRouteId)
{
    auto selectedRoute = m_routesRepository->route(selectedRouteId);

    if (m_selectedRoute == selectedRoute)
        return;

    m_selectedRoute = selectedRoute;
    emit selectedRouteChanged(selectedRouteId);
}

void RoutesController::addNewRoute(const QString& routeTypeId)
{
    auto routeType = m_routesRepository->routeType(routeTypeId);
    if (!routeType)
    {
        qWarning() << "Unknown route type";
        return;
    }

    QStringList routeNames;
    for (Route* route : m_routesRepository->routes())
    {
        routeNames += route->name();
    }

    auto route = new Route(routeType, utils::nameFromType(routeType->name, routeNames));
    m_routesRepository->saveRoute(route);
    this->selectRoute(route->id());
}

void RoutesController::updateRoute(const QVariant& routeId, const QJsonObject& data)
{
    Route* route = m_routesRepository->route(routeId);
    if (!route)
        return;

    route->fromVariantMap(data.toVariantMap());
    m_routesRepository->saveRoute(route);
}

void RoutesController::renameRoute(const QVariant& routeId, const QString& name)
{
    Route* route = m_routesRepository->route(routeId);
    if (!route)
        return;

    route->setName(name);
    m_routesRepository->saveRoute(route);
}

void RoutesController::removeRoute(const QVariant& routeId)
{
    Route* route = m_routesRepository->route(routeId);
    if (!route)
        return;

    if (m_selectedRoute == route)
        this->selectRoute(QVariant());

    m_routesRepository->removeRoute(route);
}

void RoutesController::addWaypoint(const QVariant& routeId, const QString& wptTypeId,
                                   const QVariantMap& args)
{
    Route* route = m_routesRepository->route(routeId);
    if (!route)
        return;

    const WaypointType* wptType = route->type()->waypointType(wptTypeId);
    if (!wptType)
        return;

    // Special case for altitude
    QVariantMap position = args;
    float altitude = route->waypointsCount()
                         ? route->waypoint(route->waypointsCount() - 1)->position().altitude()
                         : args.value(geo::altitude, 0).toFloat();
    position[geo::altitude] = altitude;

    Waypoint* wpt = new Waypoint(wptType);
    wpt->setPosition(position);
    route->addWaypoint(wpt);

    m_routesRepository->saveWaypoint(route, wpt);
}

void RoutesController::updateWaypointData(const QVariant& routeId, int index,
                                          const QJsonObject& data)
{
    Route* route = m_routesRepository->route(routeId);
    if (!route)
        return;

    Waypoint* waypoint = route->waypoint(index);
    if (!waypoint)
        return;

    waypoint->fromVariantMap(data.toVariantMap());

    // Promoute to the vehicle
    if (m_activeMission && m_activeMission->route() == route)
    {
        waypoint->setConfirmed(false);
        emit m_activeMission->operation()->uploadItem(route->waypointIndex(waypoint));
    }
    m_routesRepository->saveWaypoint(route, waypoint);
}

void RoutesController::updateWaypointCalcData(const QVariant& routeId, int index,
                                              const QJsonObject& calcData)
{
    Route* route = m_routesRepository->route(routeId);
    if (!route)
        return;

    Waypoint* waypoint = route->waypoint(index);
    if (!waypoint)
        return;

    waypoint->setCalcData(calcData.toVariantMap());
}

void RoutesController::onRouteAdded(Route* route)
{
    emit routeAdded(route->id());
    emit routeIdsChanged();

    connect(route, &Route::waypointAdded, this, [this, route](int index, RouteItem* waypoint) {
        emit waypointAdded(route->id(), index);
    });
    connect(route, &Route::waypointRemoved, this, [this, route](int index, RouteItem* waypoint) {
        emit waypointRemoved(route->id(), index);
    });
    connect(route, &Route::waypointChanged, this, [this, route](int index, RouteItem* waypoint) {
        emit waypointChanged(route->id(), index);
    });
}

void RoutesController::onRouteRemoved(Route* route)
{
    disconnect(route, nullptr, this, nullptr);

    emit routeRemoved(route->id().toString());
    emit routeIdsChanged();
}
