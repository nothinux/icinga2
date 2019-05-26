/* Icinga 2 | (c) 2012 Icinga GmbH | GPLv2+ */

#include "perfdata/opentsdbwriter.hpp"
#include "perfdata/opentsdbwriter-ti.cpp"
#include "icinga/service.hpp"
#include "icinga/checkcommand.hpp"
#include "icinga/macroprocessor.hpp"
#include "icinga/icingaapplication.hpp"
#include "icinga/compatutility.hpp"
#include "base/tcpsocket.hpp"
#include "base/configtype.hpp"
#include "base/objectlock.hpp"
#include "base/logger.hpp"
#include "base/convert.hpp"
#include "base/utility.hpp"
#include "base/perfdatavalue.hpp"
#include "base/application.hpp"
#include "base/stream.hpp"
#include "base/networkstream.hpp"
#include "base/exception.hpp"
#include "base/statsfunction.hpp"
#include <boost/algorithm/string.hpp>
#include <boost/algorithm/string/replace.hpp>

using namespace icinga;

REGISTER_TYPE(OpenTsdbWriter);

REGISTER_STATSFUNCTION(OpenTsdbWriter, &OpenTsdbWriter::StatsFunc);

void OpenTsdbWriter::OnConfigLoaded()
{
	ObjectImpl<OpenTsdbWriter>::OnConfigLoaded();

	if (!GetEnableHa()) {
		Log(LogDebug, "OpenTsdbWriter")
			<< "HA functionality disabled. Won't pause connection: " << GetName();

		SetHAMode(HARunEverywhere);
	} else {
		SetHAMode(HARunOnce);
	}
}

void OpenTsdbWriter::StatsFunc(const Dictionary::Ptr& status, const Array::Ptr&)
{
	DictionaryData nodes;

	for (const OpenTsdbWriter::Ptr& opentsdbwriter : ConfigType::GetObjectsByType<OpenTsdbWriter>()) {
		nodes.emplace_back(opentsdbwriter->GetName(), 1); //add more stats
	}

	status->Set("opentsdbwriter", new Dictionary(std::move(nodes)));
}

void OpenTsdbWriter::Resume()
{
	ObjectImpl<OpenTsdbWriter>::Resume();

	Log(LogInformation, "OpentsdbWriter")
		<< "'" << GetName() << "' resumed.";

	m_ReconnectTimer = new Timer();
	m_ReconnectTimer->SetInterval(10);
	m_ReconnectTimer->OnTimerExpired.connect(std::bind(&OpenTsdbWriter::ReconnectTimerHandler, this));
	m_ReconnectTimer->Start();
	m_ReconnectTimer->Reschedule(0);

	Service::OnNewCheckResult.connect(std::bind(&OpenTsdbWriter::CheckResultHandler, this, _1, _2));
}

/* Pause is equivalent to Stop, but with HA capabilities to resume at runtime. */
void OpenTsdbWriter::Pause()
{
	m_ReconnectTimer.reset();

	Log(LogInformation, "OpentsdbWriter")
		<< "'" << GetName() << "' paused.";

	ObjectImpl<OpenTsdbWriter>::Pause();
}

void OpenTsdbWriter::ReconnectTimerHandler()
{
	if (IsPaused())
		return;

	if (m_Stream)
		return;

	TcpSocket::Ptr socket = new TcpSocket();

	Log(LogNotice, "OpenTsdbWriter")
		<< "Reconnect to OpenTSDB TSD on host '" << GetHost() << "' port '" << GetPort() << "'.";

	try {
		socket->Connect(GetHost(), GetPort());
	} catch (std::exception&) {
		Log(LogCritical, "OpenTsdbWriter")
			<< "Can't connect to OpenTSDB TSD on host '" << GetHost() << "' port '" << GetPort() << "'.";
		return;
	}

	m_Stream = new NetworkStream(socket);
}

void OpenTsdbWriter::CheckResultHandler(const Checkable::Ptr& checkable, const CheckResult::Ptr& cr)
{
	if (IsPaused())
		return;

	CONTEXT("Processing check result for '" + checkable->GetName() + "'");

	if (!IcingaApplication::GetInstance()->GetEnablePerfdata() || !checkable->GetEnablePerfdata())
		return;

	Service::Ptr service = dynamic_pointer_cast<Service>(checkable);
	Host::Ptr host;

	if (service)
		host = service->GetHost();
	else
		host = static_pointer_cast<Host>(checkable);

	String metric;
	std::map<String, String> tags;

	String escaped_hostName = EscapeTag(host->GetName());
	tags["host"] = escaped_hostName;

	double ts = cr->GetExecutionEnd();

	if (service) {
		String serviceName = service->GetShortName();
		String escaped_serviceName = EscapeMetric(serviceName);
		metric = "icinga.service." + escaped_serviceName;

		SendMetric(checkable, metric + ".state", tags, service->GetState(), ts);
	} else {
		metric = "icinga.host";
		SendMetric(checkable, metric + ".state", tags, host->GetState(), ts);
	}

	SendMetric(checkable, metric + ".state_type", tags, checkable->GetStateType(), ts);
	SendMetric(checkable, metric + ".reachable", tags, checkable->IsReachable(), ts);
	SendMetric(checkable, metric + ".downtime_depth", tags, checkable->GetDowntimeDepth(), ts);
	SendMetric(checkable, metric + ".acknowledgement", tags, checkable->GetAcknowledgement(), ts);

	SendPerfdata(checkable, metric, tags, cr, ts);

	metric = "icinga.check";

	if (service) {
		tags["type"] = "service";
		String serviceName = service->GetShortName();
		String escaped_serviceName = EscapeTag(serviceName);
		tags["service"] = escaped_serviceName;
	} else {
		tags["type"] = "host";
	}

	SendMetric(checkable, metric + ".current_attempt", tags, checkable->GetCheckAttempt(), ts);
	SendMetric(checkable, metric + ".max_check_attempts", tags, checkable->GetMaxCheckAttempts(), ts);
	SendMetric(checkable, metric + ".latency", tags, cr->CalculateLatency(), ts);
	SendMetric(checkable, metric + ".execution_time", tags, cr->CalculateExecutionTime(), ts);
}

void OpenTsdbWriter::SendPerfdata(const Checkable::Ptr& checkable, const String& metric,
	const std::map<String, String>& tags, const CheckResult::Ptr& cr, double ts)
{
	Array::Ptr perfdata = cr->GetPerformanceData();

	if (!perfdata)
		return;

	CheckCommand::Ptr checkCommand = checkable->GetCheckCommand();

	ObjectLock olock(perfdata);
	for (const Value& val : perfdata) {
		PerfdataValue::Ptr pdv;

		if (val.IsObjectType<PerfdataValue>())
			pdv = val;
		else {
			try {
				pdv = PerfdataValue::Parse(val);
			} catch (const std::exception&) {
				Log(LogWarning, "OpenTsdbWriter")
					<< "Ignoring invalid perfdata for checkable '"
					<< checkable->GetName() << "' and command '"
					<< checkCommand->GetName() << "' with value: " << val;
				continue;
			}
		}

		String escaped_key = EscapeMetric(pdv->GetLabel());
		boost::algorithm::replace_all(escaped_key, "::", ".");

		SendMetric(checkable, metric + "." + escaped_key, tags, pdv->GetValue(), ts);

		if (pdv->GetCrit())
			SendMetric(checkable, metric + "." + escaped_key + "_crit", tags, pdv->GetCrit(), ts);
		if (pdv->GetWarn())
			SendMetric(checkable, metric + "." + escaped_key + "_warn", tags, pdv->GetWarn(), ts);
		if (pdv->GetMin())
			SendMetric(checkable, metric + "." + escaped_key + "_min", tags, pdv->GetMin(), ts);
		if (pdv->GetMax())
			SendMetric(checkable, metric + "." + escaped_key + "_max", tags, pdv->GetMax(), ts);
	}
}

void OpenTsdbWriter::SendMetric(const Checkable::Ptr& checkable, const String& metric,
	const std::map<String, String>& tags, double value, double ts)
{
	String tags_string = "";

	for (const Dictionary::Pair& tag : tags) {
		tags_string += " " + tag.first + "=" + Convert::ToString(tag.second);
	}

	std::ostringstream msgbuf;
	/*
	 * must be (http://opentsdb.net/docs/build/html/user_guide/writing.html)
	 * put <metric> <timestamp> <value> <tagk1=tagv1[ tagk2=tagv2 ...tagkN=tagvN]>
	 * "tags" must include at least one tag, we use "host=HOSTNAME"
	 */
	msgbuf << "put " << metric << " " << static_cast<long>(ts) << " " << Convert::ToString(value) << " " << tags_string;

	Log(LogDebug, "OpenTsdbWriter")
		<< "Checkable '" << checkable->GetName() << "' adds to metric list: '" << msgbuf.str() << "'.";

	/* do not send \n to debug log */
	msgbuf << "\n";
	String put = msgbuf.str();

	ObjectLock olock(this);

	if (!m_Stream)
		return;

	try {
		m_Stream->Write(put.CStr(), put.GetLength());
	} catch (const std::exception& ex) {
		Log(LogCritical, "OpenTsdbWriter")
			<< "Cannot write to OpenTSDB TSD on host '" << GetHost() << "' port '" << GetPort() + "'.";

		m_Stream.reset();
	}
}

/* for metric and tag name rules, see
 * http://opentsdb.net/docs/build/html/user_guide/writing.html#metrics-and-tags
 */
String OpenTsdbWriter::EscapeTag(const String& str)
{
	String result = str;

	boost::replace_all(result, " ", "_");
	boost::replace_all(result, "\\", "_");

	return result;
}

String OpenTsdbWriter::EscapeMetric(const String& str)
{
	String result = str;

	boost::replace_all(result, " ", "_");
	boost::replace_all(result, ".", "_");
	boost::replace_all(result, "\\", "_");
	boost::replace_all(result, ":", "_");

	return result;
}
