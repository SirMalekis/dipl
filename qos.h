#pragma once
// ---------------------------------------------------------------------------
// qos.h — классы качества обслуживания МСЭ-Т Y.1541 и физическая модель
// звена на основе системы массового обслуживания M/M/1.
//
// Модуль инкапсулирует:
//   * таблицу 1 и таблицу 3 Рекомендации Y.1541 (пороговые значения
//     IPTD / IPDV / IPLR / IPER для классов 0..7);
//   * пересчёт пропускной способности и текущей загрузки канала в
//     среднюю задержку, вероятность потери и изменение задержки пакета
//     по формулам M/M/1 и M/M/1/K;
//   * проверку соответствия пути требованиям выбранного класса QoS.
// ---------------------------------------------------------------------------

#include "topology.h"

#include <limits>
#include <string>

namespace netsim {

// Классы QoS: 0..5 — таблица 1/Y.1541, 6..7 — условные классы (таблица 3).
enum class QosClass : int {
    Class0 = 0,
    Class1 = 1,
    Class2 = 2,
    Class3 = 3,
    Class4 = 4,
    Class5 = 5,
    Class6 = 6,
    Class7 = 7
};

inline constexpr double kUndefined = std::numeric_limits<double>::infinity();

// Задержка, которой помечается перегруженное (ρ ≥ 1) звено. Звено остаётся
// достижимым для алгоритма Дейкстры, но заведомо не проходит ни один класс
// QoS; это позволяет отделить «нет маршрута» от «маршрут перегружен».
inline constexpr double kSaturatedDelay = 1.0e6;  // секунд

struct QosRequirement {
    const char* id;            // "0".."7"
    const char* name;          // "Класс 0"
    const char* applications;  // примеры приложений
    double      iptd_s;        // верхняя граница IPTD, с (kUndefined = "Н")
    double      ipdv_s;        // верхняя граница IPDV, с
    double      iplr;          // верхняя граница IPLR
    double      iper;          // верхняя граница IPER
    bool        conditional;   // true для классов 6 и 7
};

// Таблица требований Y.1541.
const QosRequirement& qosRequirement(QosClass cls);

// Разбор класса из числа/строки. Возвращает false при ошибке.
bool parseQosClass(const std::string& text, QosClass& out);
QosClass qosClassFromInt(int value);
const char* qosClassName(QosClass cls);
std::string qosTableText();

// Параметры физической модели.
struct QosParams {
    QosClass qos_class      = QosClass::Class1;
    double   packet_bits    = 12000.0;  // 1500 байт — размер для оценки Y.1541
    int      buffer_packets = 100;      // ёмкость очереди M/M/1/K, пакетов
};

// Состояние звена после подстановки текущей загрузки.
struct EdgeQos {
    double delay_s   = 0.0;   // средняя задержка IPTD звена
    double loss      = 0.0;   // вероятность потери IPLR звена
    double ipdv_s    = 0.0;   // оценка IPDV звена (1−10^-3 квантиль)
    double utilization = 0.0; // ρ = λ/μ
    bool   saturated = false; // ρ ≥ 1 либо C ≤ 0
};

// Пересчёт звена в терминах M/M/1(K).
EdgeQos evaluateEdgeQos(const Edge& edge, const QosParams& params);

// Проверка одного пути по требованиям класса. Значения delay/loss/ipdv
// уже агрегированы по пути (см. traffic.cpp).
struct PathQos {
    double delay_s = kUndefined;
    double loss    = 1.0;
    double ipdv_s  = kUndefined;
    bool   reachable = false;
    bool   iptd_ok   = false;
    bool   iplr_ok   = false;
    bool   ipdv_ok   = false;
    bool   conforms  = false;  // все определённые требования выполнены
};

PathQos evaluatePathQos(double delay_s, double loss, double ipdv_s,
                        bool reachable, const QosParams& params);

// Строковое представление порогов класса (для журналов и GUI).
std::string qosRequirementText(QosClass cls);

}  // namespace netsim
