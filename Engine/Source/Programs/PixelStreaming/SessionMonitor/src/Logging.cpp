// Copyright Epic Games, Inc. All Rights Reserved.

#include "SessionMonitorPCH.h"
#include "Logging.h"
#include "StringUtils.h"
#include "TimeUtils.h"

EG_DEFINE_LOG_CATEGORY(LogDefault)

const char* LogVerbosityToString(ELogVerbosity v)
{
	switch (v)
	{
	case ELogVerbosity::None:
		return "NNN";
	case ELogVerbosity::Fatal:
		return "FTL";
	case ELogVerbosity::Error:
		return "ERR";
	case ELogVerbosity::Warning:
		return "WRN";
	case ELogVerbosity::Log:
		return "LOG";
	case ELogVerbosity::Verbose:
		return "LOG";
	case ELogVerbosity::VeryVerbose:
		return "LOG";
	};
	return "Unknown";
}

FLogCategoryBase::FLogCategoryBase(const char* Name, ELogVerbosity Verbosity, ELogVerbosity CompileTimeVerbosity)
    : Name(Name)
    , Verbosity(Verbosity)
    , CompileTimeVerbosity(CompileTimeVerbosity)
{
}

bool FLogCategoryBase::IsSuppressed(ELogVerbosity V) const
{
	return V > this->Verbosity;
}

void FLogCategoryBase::SetVerbosity(ELogVerbosity V)
{
	Verbosity = ELogVerbosity(std::min((int)CompileTimeVerbosity, (int)V));
}

//////////////////////////////////////////////////////////////////////////
// ILogOutput
//////////////////////////////////////////////////////////////////////////

ILogOutput::ILogOutput()
{
	FSharedData* Data = GetSharedData();
	auto Lk = std::unique_lock<std::mutex>(Data->Mtx);
	Data->Outputs.push_back(this);
}

ILogOutput::~ILogOutput()
{
	FSharedData* Data = GetSharedData();
	auto Lk = std::unique_lock<std::mutex>(Data->Mtx);
	Data->Outputs.erase(std::find(Data->Outputs.begin(), Data->Outputs.end(), this));
}

ILogOutput::FSharedData* ILogOutput::GetSharedData()
{
	// This is thread safe (aka: Magic statics in C++11)
	static FSharedData Data;
	return &Data;
}

void ILogOutput::LogToAll(
    const char* File, int Line, const FLogCategoryBase* Category, ELogVerbosity Verbosity,
    _Printf_format_string_ const char* Fmt, ...)
{
	va_list Args;
	va_start(Args, Fmt);

	const char* Prefix = "";

	{
		FDateTime DateTime = PARAM_LocalTime ? Now() : UtcNow();
		Prefix = FormatString(
		    "[%s]: %s: %-11s: ",
			DateTime.ToString(),
		    LogVerbosityToString(Verbosity),
		    Category->Name.c_str());
	}

	char* Msg = FormatStringVA(Fmt, Args);
	constexpr int BufSize = 1024*10;
	char Buf[BufSize];
	SNPrintf(Buf, BufSize, "%s%s\n", Prefix, Msg);

	OutputDebugStringA(Buf);

	FSharedData* Data = GetSharedData();
	auto Lk = std::unique_lock<std::mutex>(Data->Mtx);
	for (ILogOutput* Out : Data->Outputs)
	{
		Out->Log(File, Line, Category, Verbosity, Buf);
	}
}
