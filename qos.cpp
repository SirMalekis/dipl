#include "qos.h"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <iomanip>
#include <sstream>
#include <stdexcept>

namespace netsim {

namespace {

const QosRequirement kRequirements[8] = {
    // id, name, applications, IPTD, IPDV, IPLR, IPER, conditional
    {"0", "Класс 0", "VoIP, видеоконференции (интерактивные, чувствительные к дрожанию)",
     0.100, 0.050, 1.0e-3, 1.0e-4, false},
    {"1", "Класс 1", "VoIP, видеоконференции (интерактивные)",
     0.400, 0.050, 1.0e-3, 1.0e-4, false},
    {"2", "Класс 2", "Сигнализация, транзакционные данные с высокой интерактивностью",
     0.100, kUndefined, 1.0e-3, 1.0e-4, false},
    {"3", "Класс 3", "Транзакционные данные, интерактивные приложения",
     0.400, kUndefined, 1.0e-3, 1.0e-4, false},
    {"4", "Класс 4", "Массовая передача данных, короткие транзакции, видео",
     1.000, kUndefined, 1.0e-3, 1.0e-4, false},
    {"5", "Класс 5", "Традиционные приложения IP-сетей (best effort)",
     kUndefined, kUndefined, kUndefined, kUndefined, false},
    {"6", "Класс 6", "Цифровое телевидение, данные с повышенными требованиями к потерям",
     0.100, 0.050, 1.0e-5, 1.0e-6, true},
    {"7", "Класс 7", "Цифровое телевидение, данные с повышенными требованиями к потерям",
     0.400, 0.050, 1.0e-5, 1.0e-6, true},
};

std::string formatSeconds(double seconds) {
    if (!std::isfinite(seconds)) return "Н";
    std::ostringstream oss;
    if (seconds < 1.0) {
        oss << std::fixed << std::setprecision(0) << seconds * 1000.0 << " мс";
    } else {
        oss << std::fixed << std::setprecision(1) << seconds << " с";
    }
    return oss.str();
}

std::string formatProbability(double p) {
    if (!std::isfinite(p)) return "Н";
    std::ostringstream oss;
    oss << std::scientific << std::setprecision(0) << p;
    return oss.str();
}

bool isDefined(double value) {
    return std::isfinite(value);
}

double clamp01(double value) {
    return std::max(0.0, std::min(1.0, value));
}

}  // namespace

const QosRequirement& qosRequirement(QosClass cls) {
    const int idx = static_cast<int>(cls);
    if (idx < 0 || idx > 7)
        throw std::out_of_range("Недопустимый класс QoS: " + std::to_string(idx));
    return kRequirements[idx];
}

QosClass qosClassFromInt(int value) {
    if (value < 0 || value > 7)
        throw std::out_of_range("Класс QoS должен быть в диапазоне 0..7");
    return static_cast<QosClass>(value);
}

bool parseQosClass(const std::string& text, QosClass& out) {
    std::string s;
    for (char c : text) {
        if (!std::isspace(static_cast<unsigned char>(c))) s.push_back(c);
    }
    if (s.size() >= 5 && s.compare(0, 5, "class") == 0) s = s.substr(5);
    if (s.size() == 1 && s[0] >= '0' && s[0] <= '7') {
        out = static_cast<QosClass>(s[0] - '0');
        return true;
    }
    return false;
}

const char* qosClassName(QosClass cls) {
    return qosRequirement(cls).name;
}

std::string qosRequirementText(QosClass cls) {
    const auto& r = qosRequirement(cls);
    std::ostringstream oss;
    oss << r.name << ": IPTD ≤ " << formatSeconds(r.iptd_s)
        << ", IPDV ≤ " << formatSeconds(r.ipdv_s)
        << ", IPLR ≤ " << formatProbability(r.iplr)
        << ", IPER ≤ " << formatProbability(r.iper);
    if (r.conditional) oss << " (условный класс)";
    return oss.str();
}

std::string qosTableText() {
    std::ostringstream oss;
    oss << "Классы сетевого QoS по Рекомендации МСЭ-Т Y.1541 (таблицы 1 и 3)\n";
    oss << "------------------------------------------------------------------------------------------\n";
    oss << std::left
        << std::setw(9)  << "Класс"
        << std::setw(14) << "IPTD"
        << std::setw(14) << "IPDV"
        << std::setw(14) << "IPLR"
        << std::setw(14) << "IPER"
        << "Приложения\n";
    oss << "------------------------------------------------------------------------------------------\n";
    for (int i = 0; i < 8; ++i) {
        const auto& r = kRequirements[i];
        oss << std::left
            << std::setw(9)  << r.name
            << std::setw(14) << formatSeconds(r.iptd_s)
            << std::setw(14) << formatSeconds(r.ipdv_s)
            << std::setw(14) << formatProbability(r.iplr)
            << std::setw(14) << formatProbability(r.iper)
            << r.applications;
        if (r.conditional) oss << " [условный]";
        oss << "\n";
    }
    oss << "------------------------------------------------------------------------------------------\n";
    oss << "«Н» — требование не определено (unbounded). Классы 6–7 — условные (таблица 3).\n";
    return oss.str();
}

EdgeQos evaluateEdgeQos(const Edge& edge, const QosParams& params) {
    EdgeQos out;

    const double packet_bits = params.packet_bits > 0.0 ? params.packet_bits : 12000.0;
    if (edge.capacity <= 0.0) {
        out.saturated = true;
        out.delay_s = kSaturatedDelay;
        out.loss = 1.0;
        out.ipdv_s = 0.0;
        out.utilization = std::numeric_limits<double>::infinity();
        return out;
    }

    const double mu = edge.capacity / packet_bits;      // пакетов/с
    const double lambda = std::max(0.0, edge.load) / packet_bits;
    const double rho = (mu > 0.0) ? lambda / mu : kUndefined;
    out.utilization = rho;

    // Условие устойчивости M/M/1: ρ < 1. При ρ ≥ 1 очередь неограниченно
    // растёт, поэтому звено помечается перегруженным.
    if (!std::isfinite(rho) || rho >= 1.0 - 1.0e-12) {
        out.saturated = true;
        out.delay_s = kSaturatedDelay;
        out.loss = 1.0;
        out.ipdv_s = 0.0;
        return out;
    }

    // Среднее время пребывания заявки в M/M/1: T = 1/(μ − λ).
    const double sojourn = 1.0 / (mu - lambda);
    out.delay_s = edge.base_latency + sojourn;

    // Вероятность блокировки M/M/1/K: P_K = (1−ρ)ρ^K / (1−ρ^(K+1)).
    const int K = std::max(1, params.buffer_packets);
    if (rho <= 1.0e-12) {
        out.loss = 0.0;
    } else if (std::abs(1.0 - rho) <= 1.0e-12) {
        out.loss = 1.0 / static_cast<double>(K + 1);
    } else {
        const double rho_k = std::pow(rho, K);
        const double rho_k1 = rho_k * rho;
        out.loss = (1.0 - rho) * rho_k / (1.0 - rho_k1);
    }
    out.loss = clamp01(out.loss);

    // IPDV как верхняя граница 1−10^-3 квантиля задержки за вычетом
    // минимального значения. Для экспоненциального распределения времени
    // пребывания в M/M/1: t_p = −ln(1−p)/(μ−λ).
    constexpr double kQuantile = 1.0e-3;
    out.ipdv_s = -std::log(kQuantile) / (mu - lambda);

    return out;
}

PathQos evaluatePathQos(double delay_s, double loss, double ipdv_s,
                        bool reachable, const QosParams& params) {
    PathQos out;
    out.reachable = reachable;
    out.delay_s = reachable ? delay_s : kUndefined;
    out.loss = reachable ? clamp01(loss) : 1.0;
    out.ipdv_s = reachable ? ipdv_s : kUndefined;

    const auto& req = qosRequirement(params.qos_class);
    out.iptd_ok = !isDefined(req.iptd_s) || (reachable && delay_s <= req.iptd_s);
    out.iplr_ok = !isDefined(req.iplr) || (reachable && loss <= req.iplr);
    out.ipdv_ok = !isDefined(req.ipdv_s) || (reachable && ipdv_s <= req.ipdv_s);
    out.conforms = reachable && out.iptd_ok && out.iplr_ok && out.ipdv_ok;
    return out;
}

}  // namespace netsim
