#include "vector/model.hpp"
#include <algorithm>
#include <iostream>
#include <stdexcept>
using namespace vectorapp;
int checks=0;
void check(bool ok,const char* message){++checks;if(!ok)throw std::runtime_error(message);}
int main() {
    try {
        auto read=[](const std::string& reply){Service s;s.port=1900;std::vector<Finding> f;read_udp_reply("ssdp",reply,s,f);return f;};
        auto gateway=[](const auto& f){return std::find_if(f.begin(),f.end(),[](const Finding& x){return x.name=="Router / gateway";});};
        for(const auto* version:{"1","2"}) {
            const std::string type=std::string("urn:schemas-upnp-org:device:InternetGatewayDevice:")+version;
            auto f=read("HTTP/1.1 200 OK\r\nSERVER: Linux/5.15 UPnP/1.1\r\nST: "+type+"\r\n\r\n");
            auto it=gateway(f);check(it!=f.end(),"gateway device type identified");
            check(it->category=="Device hint"&&it->confidence=="Medium","advertised role is a device hint");
            check(it->version.empty(),"UPnP device version is not firmware");
            check(it->evidence.size()==1&&it->evidence[0].source=="UDP 1900 ssdp | ST"&&it->evidence[0].value==type,"exact type evidence retained");
        }
        for(const auto* type:{"upnp:rootdevice","uuid:gateway","urn:schemas-upnp-org:device:MediaRenderer:1",
            "urn:schemas-upnp-org:service:WANIPConnection:1","urn:example.com:device:InternetGatewayDevice:1",
            "urn:schemas-upnp-org:device:InternetGatewayDevice:1evil","a router urn:schemas-upnp-org:device:InternetGatewayDevice:1"}) {
            auto f=read(std::string("HTTP/1.1 200 OK\r\nST: ")+type+"\r\n\r\n");
            check(gateway(f)==f.end(),"generic devices and misleading type strings are not routers");
        }
        for(const auto* reply:{
            "HTTP/1.1 404 Missing\r\nST: urn:schemas-upnp-org:device:InternetGatewayDevice:1\r\n\r\n",
            "not a response\r\nST: urn:schemas-upnp-org:device:InternetGatewayDevice:1\r\n\r\n",
            "HTTP/1.1 200 OK\r\nST: upnp:rootdevice\r\n\r\nST: urn:schemas-upnp-org:device:InternetGatewayDevice:1",
            "HTTP/1.1 200 OK\r\nSERVER: router\r\n\r\nInternetGatewayDevice:1"}) {
            auto f=read(reply);check(gateway(f)==f.end(),"body text and error packets do not identify a router");
        }
        Service s;s.port=53;s.state="open";s.active_probe=true;std::vector<Finding> f;
        identify_banner(s,f);check(s.name=="Unknown"&&f.empty(),"actual empty TCP53 result cannot establish DNS or router identity");

        const std::string desc =
            "<?xml version=\"1.0\"?>"
            "<root><device>"
            "<deviceType>urn:schemas-upnp-org:device:InternetGatewayDevice:1</deviceType>"
            "<friendlyName>Vector Test Gateway</friendlyName>"
            "<manufacturer>VectorCo</manufacturer>"
            "<modelName>EdgeBox</modelName>"
            "<modelNumber>42</modelNumber>"
            "</device></root>";
        std::vector<Finding> desc_findings;
        absorb_upnp_description(desc, "http://192.0.2.1/rootDesc.xml", desc_findings);
        auto has = [&](const char* name) {
            return std::find_if(desc_findings.begin(), desc_findings.end(),
                [&](const Finding& x) { return x.name == name; }) != desc_findings.end();
        };
        check(has("Router / gateway"), "deviceType from description identifies gateway");
        check(has("UPnP friendly name") && has("UPnP manufacturer") && has("UPnP model"), "description identity fields retained");
        auto model = std::find_if(desc_findings.begin(), desc_findings.end(),
            [](const Finding& x) { return x.name == "UPnP model number"; });
        check(model != desc_findings.end() && model->version == "42", "modelNumber becomes finding version");

        const std::string with_location =
            "HTTP/1.1 200 OK\r\nSERVER: Linux UPnP/1.1\r\n"
            "LOCATION: http://192.0.2.1:5000/rootDesc.xml\r\n"
            "ST: urn:schemas-upnp-org:device:InternetGatewayDevice:1\r\n\r\n";
        Service loc; loc.port = 1900;
        std::vector<Finding> loc_findings;
        std::vector<std::string> locations;
        read_udp_reply("ssdp", with_location, loc, loc_findings, &locations);
        check(locations.size() == 1 && locations[0].find("rootDesc.xml") != std::string::npos, "LOCATION collected for follow-up");
        check(std::find_if(loc_findings.begin(), loc_findings.end(),
            [](const Finding& x) { return x.name == "UPnP location"; }) != loc_findings.end(), "LOCATION recorded as evidence");

        {
            Service lib; lib.port = 1900;
            std::vector<Finding> lib_findings;
            read_udp_reply("ssdp",
                "HTTP/1.1 200 OK\r\nSERVER: Linux/4.4 UPnP/1.1 libupnp/1.6.22\r\n"
                "ST: upnp:rootdevice\r\nLOCATION: http://192.168.1.1:5000/rootDesc.xml\r\n\r\n",
                lib, lib_findings);
            check(std::find_if(lib_findings.begin(), lib_findings.end(),
                [](const Finding& x) { return x.name == "UPnP Portable SDK"; }) != lib_findings.end(),
                "libupnp SERVER becomes a Device hint");
            check(std::find_if(lib_findings.begin(), lib_findings.end(),
                [](const Finding& x) { return x.name == "UPnP location"; }) != lib_findings.end(),
                "private LOCATION still recorded from SSDP");
        }
        {
            Service mini; mini.port = 1900;
            std::vector<Finding> mini_findings;
            read_udp_reply("ssdp",
                "HTTP/1.1 200 OK\r\nSERVER: FreeBSD/14.3 UPnP/1.1 MiniUPnPd/2.3.9\r\n"
                "ST: urn:schemas-upnp-org:device:InternetGatewayDevice:1\r\n\r\n",
                mini, mini_findings);
            check(std::find_if(mini_findings.begin(), mini_findings.end(),
                [](const Finding& x) { return x.name == "MiniUPnP"; }) != mini_findings.end(),
                "MiniUPnPd SERVER becomes a Device hint");
            check(gateway(mini_findings) != mini_findings.end(), "IGD ST still yields Router / gateway");
        }
        {
            const std::string descr = "Cisco IOS Software, C2900 Software (C2900-UNIVERSALK9-M)";
            std::string snmp_reply = "padding";
            snmp_reply.push_back(static_cast<char>(0x04));
            snmp_reply.push_back(static_cast<char>(descr.size()));
            snmp_reply += descr;
            Service snmp; snmp.port = 161;
            std::vector<Finding> snmp_findings;
            read_udp_reply("snmp", snmp_reply, snmp, snmp_findings);
            check(std::find_if(snmp_findings.begin(), snmp_findings.end(),
                [](const Finding& x) { return x.name == "Cisco IOS"; }) != snmp_findings.end(),
                "Cisco IOS sysDescr becomes a Device hint");
        }

        const std::string desc_xml =
            "<?xml version=\"1.0\"?><root><URLBase>http://192.0.2.1:5000</URLBase><device>"
            "<serviceList><service>"
            "<serviceType>urn:schemas-upnp-org:service:WANIPConnection:1</serviceType>"
            "<controlURL>/ctl/IPConn</controlURL>"
            "</service></serviceList></device></root>";
        std::string service_type;
        const auto control=upnp_service_control_url(desc_xml,"http://192.0.2.1:5000/rootDesc.xml",&service_type);
        check(control=="http://192.0.2.1:5000/ctl/IPConn","WANIPConnection controlURL resolved");
        check(service_type=="urn:schemas-upnp-org:service:WANIPConnection:1","service type retained for SOAPAction");
        const auto soap=upnp_port_mapping_soap(0,service_type);
        check(soap.find("GetGenericPortMappingEntry")!=std::string::npos&&soap.find("<NewPortMappingIndex>0</NewPortMappingIndex>")!=std::string::npos,"SOAP index body");
        const std::string mapping_xml=
            "<s:Envelope><s:Body><u:GetGenericPortMappingEntryResponse>"
            "<NewRemoteHost></NewRemoteHost><NewExternalPort>554</NewExternalPort><NewProtocol>TCP</NewProtocol>"
            "<NewInternalPort>554</NewInternalPort><NewInternalClient>198.51.100.50</NewInternalClient>"
            "<NewEnabled>1</NewEnabled><NewPortMappingDescription>Camera</NewPortMappingDescription>"
            "<NewLeaseDuration>0</NewLeaseDuration>"
            "</u:GetGenericPortMappingEntryResponse></s:Body></s:Envelope>";
        std::string client,protocol,ext,inner,mapping_desc;
        check(upnp_mapping_from_soap(mapping_xml,client,protocol,ext,inner,mapping_desc),"mapping SOAP parses");
        check(client=="198.51.100.50"&&protocol=="TCP"&&ext=="554"&&inner=="554"&&mapping_desc=="Camera","mapping fields retained");
        check(!upnp_mapping_from_soap("<fault><errorCode>713</errorCode><errorDescription>SpecifiedArrayIndexInvalid</errorDescription></fault>",client,protocol,ext,inner,mapping_desc),
            "index exhausted stops the walk");
        std::cout<<checks<<" router evidence checks passed.\n";
        return 0;
    }catch(const std::exception& e){std::cerr<<"FAIL: "<<e.what()<<'\n';return 1;}
}
