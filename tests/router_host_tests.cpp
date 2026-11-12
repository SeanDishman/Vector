#include "vector/model.hpp"
#include <iostream>
#include <stdexcept>
using namespace vectorapp;
int checks = 0;
void check(bool ok, const char* message) { ++checks; if (!ok) throw std::runtime_error(message); }
int main() {
    try {
        const std::string xml =
            "<?xml version=\"1.0\"?><root><device><serviceList>"
            "<service><serviceType>urn:schemas-upnp-org:service:WANIPConnection:1</serviceType>"
            "<controlURL>/ctl/IPConn</controlURL></service>"
            "<service><serviceType>urn:dslforum-org:service:Hosts:1</serviceType>"
            "<controlURL>/upnp/control/hosts</controlURL></service>"
            "</serviceList></device></root>";
        const auto control = router_hosts_control_url(xml, "http://192.0.2.1:49000/tr64desc.xml");
        check(control.find("192.0.2.1") != std::string::npos && control.find("/upnp/control/hosts") != std::string::npos,
            "Hosts controlURL resolved against the description");
        check(router_hosts_control_url(
            "<service><serviceType>urn:schemas-upnp-org:service:WANIPConnection:1</serviceType><controlURL>/ctl</controlURL></service>",
            "http://192.0.2.1/desc.xml").empty(),
            "WANIPConnection is not the host table");
        const auto with_base =
            "<URLBase>http://192.0.2.1:49000/</URLBase><service>"
            "<serviceType>urn:dslforum-org:service:Hosts:1</serviceType><controlURL>hosts</controlURL></service>";
        check(router_hosts_control_url(with_base, "http://192.0.2.1/ignored.xml").find(":49000/") != std::string::npos,
            "URLBase wins over the description URL");

        const auto soap = router_host_soap(3, "urn:dslforum-org:service:Hosts:1");
        check(soap.find("<NewIndex>3</NewIndex>") != std::string::npos && soap.find("GetGenericHostEntry") != std::string::npos,
            "host-entry SOAP carries the index");
        check(router_host_soap(1, "not a urn").find("urn:dslforum-org:service:Hosts:1") != std::string::npos,
            "unexpected service type is not interpolated");

        std::string ip, mac, name, active;
        const std::string row =
            "<u:GetGenericHostEntryResponse>"
            "<NewIPAddress>192.0.2.23</NewIPAddress><NewMACAddress>AA:BB:CC:DD:EE:01</NewMACAddress>"
            "<NewHostName>camera</NewHostName><NewActive>1</NewActive>"
            "</u:GetGenericHostEntryResponse>";
        check(router_host_from_soap(row, ip, mac, name, active) && ip == "192.0.2.23" && mac == "AA:BB:CC:DD:EE:01" &&
            name == "camera" && active == "1", "host entry fields");
        check(!router_host_from_soap(
            "<fault><errorCode>713</errorCode><errorDescription>SpecifiedArrayIndexInvalid</errorDescription></fault>",
            ip, mac, name, active), "index fault stops the walk");
        check(!router_host_from_soap("the lan has 192.0.2.23 and a camera", ip, mac, name, active),
            "prose that mentions an address is not a host entry");
        check(!router_host_from_soap("<NewInternalClient>192.0.2.23</NewInternalClient>", ip, mac, name, active),
            "a NAT mapping client is not a host-table row");

        std::cout << checks << " router host-table checks passed.\n";
        return 0;
    } catch (const std::exception& e) {
        std::cerr << "FAIL: " << e.what() << '\n';
        return 1;
    }
}
