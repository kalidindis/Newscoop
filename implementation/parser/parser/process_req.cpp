/******************************************************************************

CAMPSITE is a Unicode-enabled multilingual web content
management system for news publications.
CAMPFIRE is a Unicode-enabled java-based near WYSIWYG text editor.
Copyright (C)2000,2001  Media Development Loan Fund
contact: contact@campware.org - http://www.campware.org
Campware encourages further development. Please let us know.

This program is free software; you can redistribute it and/or
modify it under the terms of the GNU General Public License
as published by the Free Software Foundation; either version 2
of the License, or (at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, write to the Free Software
Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.

******************************************************************************/

/******************************************************************************

Implementation of functions for client request processing

******************************************************************************/

#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sstream>

#include "process_req.h"
#include "parser.h"
#include "util.h"
#include "auto_ptr.h"
#include "curl.h"

using std::stringstream;
using std::endl;

// RunParser:
//   - prepare the context: read cgi environment into context, read user subscriptions
//     into context
//   - perform requested actions: log user in, add user to database, add subscriptions,
//     modify user informations, search keywords against database
//   - launch parser with current context: search for a parser instance of the requested
//     template; if not found, create parser instance for requested template and add it
//     to parsers hash; call Parse and WriteOutput methods of parser instance; eventually
//     call PrintParseErrors and PrintWriteErrors for admin users
// Return RES_OK if no errors occured, error code otherwise
// Parameters:
//		MYSQL* p_pSql - pointer to MySQL connection
//		CURL* p_pURL - pointer to the URL object
//		const char* p_pchRemoteIP - pointer to string containing the client IP address
//		sockstream& p_rOs - output stream
int RunParser(MYSQL* p_pSQL, CURL* p_pcoURL, const char* p_pchRemoteIP, sockstream& p_rOs)
	throw(RunException, bad_alloc)
{
	if (p_pcoURL == NULL)
		throw RunException("Invalid params");
	if (p_pSQL == NULL)
		throw RunException("MYSQL connection not initialised");
	static const char* ppchParams[PARAM_NR] =
	    {
	        "Name", "EMail", "CountryCode", "UName", "Password", "PasswordAgain",
	        "City", "StrAddress", "State", "Phone", "Fax", "Contact", "Phone2",
	        "Title", "Gender", "Age", "PostalCode", "Employer", "EmployerType",
	        "Position", "Interests", "How", "Languages", "Improvements", "Pref1",
	        "Pref2", "Pref3", "Pref4", "Field1", "Field2", "Field3", "Field4",
	        "Field5", "Text1", "Text2", "Text3"
	    };
	static const int errs[ERR_NR] =
	    {
	        UERR_NO_NAME, UERR_NO_EMAIL, UERR_NO_COUNTRY, UERR_NO_UNAME,
	        UERR_NO_PASSWORD, UERR_NO_PASSWORD_AGAIN
	    };
	SafeAutoPtr<CContext> pcoCtx(new CContext);
	string coStr;
	bool bDebug = false, bPreview = false, bTechDebug = false;
	char pchBuf[300];
	pcoCtx->SetIP(htonl(inet_addr(p_pchRemoteIP)));
	if ((coStr = p_pcoURL->getValue(P_IDLANG)) != "")
	{
		pcoCtx->SetLanguage(atol(coStr.c_str()));
		pcoCtx->SetDefLanguage(atol(coStr.c_str()));
	}
	if ((coStr = p_pcoURL->getValue(P_IDPUBL)) != "")
	{
		pcoCtx->SetPublication(atol(coStr.c_str()));
		pcoCtx->SetDefPublication(atol(coStr.c_str()));
	}
	if ((coStr = p_pcoURL->getValue(P_NRISSUE)) != "")
	{
		pcoCtx->SetIssue(atol(coStr.c_str()));
		pcoCtx->SetDefIssue(atol(coStr.c_str()));
	}
	if ((coStr = p_pcoURL->getValue(P_NRSECTION)) != "")
	{
		pcoCtx->SetSection(atol(coStr.c_str()));
		pcoCtx->SetDefSection(atol(coStr.c_str()));
	}
	if ((coStr = p_pcoURL->getValue(P_NRARTICLE)) != "")
	{
		pcoCtx->SetArticle(atol(coStr.c_str()));
		pcoCtx->SetDefArticle(atol(coStr.c_str()));
	}
	if ((coStr = p_pcoURL->getValue(P_TOPIC_ID)) != "")
	{
		pcoCtx->SetTopic(atol(coStr.c_str()));
		pcoCtx->SetDefTopic(atol(coStr.c_str()));
	}
	if ((coStr = p_pcoURL->getValue(P_ILSTART)) != "")
		pcoCtx->SetIListStart(atol(coStr.c_str()));
	if ((coStr = p_pcoURL->getValue(P_SLSTART)) != "")
		pcoCtx->SetSListStart(atol(coStr.c_str()));
	if ((coStr = p_pcoURL->getValue(P_ALSTART)) != "")
		pcoCtx->SetAListStart(atol(coStr.c_str()));
	if ((coStr = p_pcoURL->getValue(P_SRLSTART)) != "")
		pcoCtx->SetSrListStart(atol(coStr.c_str()));
	if ((coStr = p_pcoURL->getValue("ST_max")) != "")
	{
		int st_max = atol(coStr.c_str());
		for (int i = 1; i <= st_max; i++)
		{
			string coField, coArtType;
			sprintf(pchBuf, "ST%d", i);
			if ((coStr = p_pcoURL->getValue(pchBuf)) != "")
				coField = coStr;
			sprintf(pchBuf, "ST_T%d", i);
			if ((coStr = p_pcoURL->getValue(pchBuf)) != "")
				coArtType = coStr;
			if (coField == "" || coArtType == "")
				continue;
			pcoCtx->SetField(coField, coArtType);
			sprintf(pchBuf, "ST_LS%d", i);
			if ((coStr = p_pcoURL->getValue(pchBuf)) != "")
				pcoCtx->SetStListStart(atol(coStr.c_str()), coField);
			sprintf(pchBuf, "ST_PS%d", i);
			if ((coStr = p_pcoURL->getValue(pchBuf)) != "")
			{
				pcoCtx->SetStartSubtitle(atol(coStr.c_str()), coField);
				pcoCtx->SetDefaultStartSubtitle(atol(coStr.c_str()), coField);
			}
			sprintf(pchBuf, "ST_AS%d", i);
			if ((coStr = p_pcoURL->getValue(pchBuf)) != "")
				pcoCtx->SetAllSubtitles(atol(coStr.c_str()), coField);
		}
	}
	CheckUserInfo(p_pcoURL, *pcoCtx, ppchParams, PARAM_NR);
	bool bAccessAll = false;
	pcoCtx->SetReader(true);
	if ((coStr = p_pcoURL->getValue(P_SEARCH)) != "")
	{
		int nRes = Search(*pcoCtx, p_pSQL, p_pcoURL);
		if (nRes < SRERR_INTERNAL && nRes != 0)
			nRes = SRERR_INTERNAL;
		pcoCtx->SetSearchRes(nRes);
	}
	if ((coStr = p_pcoURL->getValue(P_USERADD)) != "")
	{
		int nRes = AddUser((*pcoCtx), p_pSQL, ppchParams, PARAM_NR, errs, ERR_NR);
		if (nRes < UERR_INTERNAL && nRes != 0)
			nRes = UERR_INTERNAL;
		pcoCtx->SetAddUserRes(nRes);
		if (nRes == 0)
			p_rOs << "<META HTTP-EQUIV=\"Set-Cookie\" CONTENT=\"TOL_UserId="
			<< pcoCtx->User() << "; path=/\">\n"
			<< "<META HTTP-EQUIV=\"Set-Cookie\" CONTENT=\"TOL_UserKey="
			<< pcoCtx->Key() << "; path=/\">\n";
	}
	else if ((coStr = p_pcoURL->getValue(P_LOGIN)) != "")
	{
		int nRes = Login(p_pcoURL, (*pcoCtx), p_pSQL);
		if (nRes < LERR_INTERNAL && nRes != 0)
			nRes = LERR_INTERNAL;
		pcoCtx->SetLoginRes(nRes);
		if (nRes == 0)
		{
			string coExpires = "";
			if ((coStr = p_pcoURL->getValue(P_REMEMBER_USER)) != "")
			{
				coExpires = "; expires=Tuesday, 31-Dec-2069 00:00:00 GMT";
			}
			p_rOs << "<META HTTP-EQUIV=\"Set-Cookie\" CONTENT=\"TOL_UserId="
			<< pcoCtx->User() << "; path=/" << coExpires << "\">\n"
			<< "<META HTTP-EQUIV=\"Set-Cookie\" CONTENT=\"TOL_UserKey="
			<< pcoCtx->Key() << "; path=/" << coExpires << "\">\n";
		}
	}
	else if (p_pcoURL->getCookies().size() > 0)
	{
		if ((coStr = p_pcoURL->getCookie("TOL_UserId")) != "")
			pcoCtx->SetUser(atol(coStr.c_str()));
		if ((coStr = p_pcoURL->getCookie("TOL_UserKey")) != "")
			pcoCtx->SetKey(strtoul(coStr.c_str(), 0, 10));
		if ((coStr = p_pcoURL->getCookie("TOL_Access")) != "")
			bAccessAll = coStr == "all";
		if ((coStr = p_pcoURL->getCookie("TOL_Preview")) != "")
			bPreview = coStr == "on";
		if ((coStr = p_pcoURL->getCookie("TOL_Debug")) != "")
			bTechDebug = coStr == "on";
	}
	if ((coStr = p_pcoURL->getValue(P_USERMODIFY)) != "")
	{
		int nRes = ModifyUser((*pcoCtx), p_pSQL, ppchParams, PARAM_NR, errs, ERR_NR);
		if (nRes < UERR_INTERNAL && nRes != 0)
			nRes = UERR_INTERNAL;
		pcoCtx->SetModifyUserRes(nRes);
	}
	if ((coStr = p_pcoURL->getValue(P_SUBSTYPE)) != "")
	{
		if (strcasecmp(coStr.c_str(), "paid") == 0)
			pcoCtx->SetSubsType(ST_PAID);
		if (strcasecmp(coStr.c_str(), "trial") == 0)
			pcoCtx->SetSubsType(ST_TRIAL);
	}
	if ((coStr = p_pcoURL->getValue(P_SUBSCRIBE)) != "")
	{
		int nRes = DoSubscribe(p_pcoURL, (*pcoCtx), p_pSQL);
		if (nRes < SERR_INTERNAL && nRes != 0)
			nRes = SERR_INTERNAL;
		pcoCtx->SetSubsRes(nRes);
	}
	long int nIdUserIP = -1;
	sprintf(pchBuf, "select IdUser from SubsByIP where StartIP <= %lu and "
	        "%lu <= (StartIP + Addresses - 1)", pcoCtx->IP(), pcoCtx->IP());
	SQLQuery(p_pSQL, pchBuf);
	StoreResult(p_pSQL, coSqlRes);
	if (mysql_num_rows(*coSqlRes) > 0)
	{
		MYSQL_ROW row = mysql_fetch_row(*coSqlRes);
		if (pcoCtx->User() < 0)
			pcoCtx->SetUser(atol(row[0]));
		else
			nIdUserIP = atol(row[0]);
		pcoCtx->SetAccessByIP(true);
	}
	if (pcoCtx->User() >= 0)
	{
		sprintf(pchBuf, "select Reader, KeyId from Users where Id = %ld", pcoCtx->User());
		SQLQuery(p_pSQL, pchBuf);
		StoreResult(p_pSQL, coSqlRes);
		MYSQL_ROW row;
		bool bHasAccess = false;
		if (mysql_num_rows(*coSqlRes) > 0)
		{
			row = mysql_fetch_row(*coSqlRes);
			pcoCtx->SetReader(row[0][0] == 'Y');
			if (row[1] != 0 && pcoCtx->Key() == strtoul(row[1], 0, 10))
			{
				bHasAccess = true;
			}
			else
				pcoCtx->SetKey(0);
		}
		else
			pcoCtx->SetUser( -1);
		if (!bHasAccess && nIdUserIP >= 0)
		{
			sprintf(pchBuf, "select Reader from Users where Id = %ld", nIdUserIP);
			SQLQuery(p_pSQL, pchBuf);
			coSqlRes = mysql_store_result(p_pSQL);
			if (mysql_num_rows(*coSqlRes) > 0)
			{
				row = mysql_fetch_row(*coSqlRes);
				pcoCtx->SetUser(nIdUserIP);
				pcoCtx->SetReader(row[0][0] == 'Y');
				pcoCtx->SetKey(0);
			}
		}
		if (bHasAccess || pcoCtx->AccessByIP())
		{
			if (bAccessAll && !pcoCtx->IsReader())
				pcoCtx->SetAccess(A_ALL);
			if (pcoCtx->IsReader())
			{
				bTechDebug = bDebug = bPreview = false;
				SetReaderAccess((*pcoCtx), p_pSQL);
			}
		}
		else
			bTechDebug = bDebug = bPreview = false;
	}
	try
	{
		CParser::setMYSQL(p_pSQL);
		CParser* p = CParser::parserOf(p_pcoURL->getPathTranslated().c_str(),
			                           p_pcoURL->getDocumentRoot().c_str());
		p->setDebug(bTechDebug);
// no need to write the charset anymore: tpl_cgi will print it
//		WriteCharset((*pcoCtx), p_pSQL, p_rOs);
		p->writeOutput(*pcoCtx, p_rOs);
		if (bPreview == true)
		{
			p_rOs << "<script LANGUAGE=\"JavaScript\">parent.e.document.open();\n"
			"parent.e.document.write(\"<html><head><title>Errors</title>"
			"</head><body bgcolor=white text=black>\\\n<pre>\\\n";
			p_rOs << "\\\n<b>Parse errors:</b>\\\n";
			p->printParseErrors(p_rOs, true);
			p_rOs << "</pre></body></html>\\\n\");\nparent.e.document.close();\n</script>\n";
		}
		CParser::setMYSQL(NULL);
	}
	catch (ExStat& rcoEx)
	{
		throw RunException("Error loading template file");
		return -1;
	}
	catch (RunException& rcoEx)
	{
		throw rcoEx;
		return -1;
	}
	catch (ExMutex& rcoEx)
	{
		throw RunException(rcoEx.Message());
		return -1;
	}
	catch (bad_alloc& rcoEx)
	{
		throw RunException("bad alloc");
		return -1;
	}
	return 0;
}

// WriteCharset: write http tag specifying the charset - according to current language
// Parameters:
//		CContext& c - current context
//		MYSQL* pSql - pointer to MySQL connection
//		sockstream& fs - output stream
int WriteCharset(CContext& c, MYSQL* pSql, sockstream& fs)
{
	if (c.Language() < 0)
		return -1;
	char pchBuf[100];
	sprintf(pchBuf, "select CodePage from Languages where Id = %ld", c.Language());
	SQLQuery(pSql, pchBuf);
	StoreResult(pSql, coSqlRes);
	CheckForRows(*coSqlRes, 1);
	FetchRow(*coSqlRes, row);
//	fs << "<META HTTP-EQUIV=\"Content-Type\" content=\"text/html; charset=" << row[0] << "\">" << endl;
	fs << "<META HTTP-EQUIV=\"Content-Type\" content=\"text/html; charset=UTF-8\">" << endl;
	return 0;
}

// Login: perform login action: log user in
// Parameters:
//		const CURL& url - url object
//		CContext& c - current context
//		MYSQL* pSql - pointer to MySQL connection
int Login(const CURL* url, CContext& c, MYSQL* pSql)
{
	c.SetLogin(true);
	string s;
	string uname, password, q;
	if ((s = url->getValue("LoginUName")) == "")
		return LERR_NO_UNAME;
	uname = s;
	if ((s = url->getValue("LoginPassword")) == "")
		return LERR_NO_PASSWORD;
	password = s;
	q = "select Password, password(\"" + password + "\") from Users where UName "
	    "= \"" + uname + "\"";
	SQLQuery(pSql, q.c_str());
	StoreResult(pSql, coSqlRes);
	if (mysql_num_rows(*coSqlRes) < 1)
	{
		return LERR_INVALID_UNAME;
	}
	FetchRow(*coSqlRes, row);
	if (strcmp(row[0], row[1]))
	{
		return LERR_INVALID_PASSWORD;
	}
	q = "update Users set KeyId = RAND()*1000000000+RAND()*1000000+RAND()*1000 "
	    "where UName = \"" + uname + "\"";
	SQLQuery(pSql, q.c_str());
	q = "select Id, KeyId from Users where UName = \"" + uname + "\"";
	SQLQuery(pSql, q.c_str());
	coSqlRes = mysql_store_result(pSql);
	CheckForRows(*coSqlRes, 1);
	row = mysql_fetch_row(*coSqlRes);
	c.SetUser(atol(row[0]));
	c.SetKey(atol(row[1]));
	return 0;
}

// CheckUserInfo: read user informations from CGI parameters
// Parameters:
//		const CURL* url - url object
//		CContext& c - current context
//		const char* ppchParams[] - parameters to read
//		int param_nr - parameters number
int CheckUserInfo(const CURL* url, CContext& c, const char* ppchParams[], int param_nr)
{
	string field_pref = "User";
	int found = 0;
	set <string, less<string> > coPrefs;
	for (int k = 1; k <= 4; k++)
	{
		stringstream coPref;
		coPref << "HasPref" << k;
		string s = url->getValue(coPref.str());
		if (s != "")
			coPrefs.insert(coPref.str().substr(3));
	}
	for (int i = 0; i < param_nr; i++)
	{
		string fld = field_pref + ppchParams[i];
		string s = url->getValue(fld);
		if (s == "")
			continue;
		c.SetUserInfo(string(ppchParams[i]), string(s));
		if (strncasecmp(ppchParams[i], "Pref", 4) == 0)
			coPrefs.erase(ppchParams[i]);
		found ++;
	}
	set <string, less<string> >::const_iterator coIt = coPrefs.begin();
	for (; coIt != coPrefs.end(); ++coIt)
		c.SetUserInfo(*coIt, "off");
	return found;
}

// AddUser: perform add user action (add user to database); return error code
// Parameters:
//		CContext& c - current context
//		MYSQL* pSql - pointer to MySQL connection
//		const char* ppchParams[] - parameters to read from context (user information)
//		int param_nr - parameters number
//		const int errs[] - error codes
//		int err_nr - errors number
int AddUser(CContext& c, MYSQL* pSql, const char* ppchParams[], int param_nr,
            const int errs[], int err_nr)
{
	c.SetAddUser(true);
	string field_pref = "User", q, fn, fv, uname, email, password;
	fn = "KeyId";
	fv = "RAND()*1000000000+RAND()*1000000+RAND()*1000";
	for (int i = 0; i < param_nr; i++)
	{
		string s = c.UserInfo(string(ppchParams[i]));
		char pchBuf[10000];
		mysql_escape_string(pchBuf, s.c_str(), strlen(s.c_str()));
		if (i < err_nr && s == "") return errs[i];
		if (s == "") continue;
		if (i == 4)
		{
			password = pchBuf;
			continue;
		}
		if (i == 1) email = s;
		if (i == 3) uname = s;
		fn += string(", ") + (i == 5 ? ppchParams[i - 1] : ppchParams[i]);
		if (i == 5)
		{
			if (password != pchBuf)
				return UERR_PASSWORDS_DONT_MATCH;
			if (strlen(s.c_str()) < 6)
				return UERR_PASSWORD_TOO_SIMPLE;
			fv += string(", password(\"") + pchBuf + "\")";
		}
		else if (strncasecmp(ppchParams[i], "Pref", 4))
			fv += string(", \"") + pchBuf + "\"";
		else
		{
			if (s == "on")
				fv += ", \"Y\"";
			else
				fv += ", \"N\"";
		}
	}
	q = "select * from Users where UName = \"" + uname + "\"";
	SQLQuery(pSql, q.c_str());
	StoreResult(pSql, coSqlRes);
	if (mysql_num_rows(*coSqlRes) > 0)
		return UERR_USER_EXISTS;
	q = "select * from Users where EMail = \"" + email + "\"";
	SQLQuery(pSql, q.c_str());
	coSqlRes = mysql_store_result(pSql);
	if (mysql_num_rows(*coSqlRes) > 0)
		return UERR_DUPLICATE_EMAIL;
	q = "insert into Users (" + fn + ") values(" + fv + ")";
	SQLQuery(pSql, q.c_str());
	q = "select Id, KeyId from Users where UName = \"" + uname + "\"";
	SQLQuery(pSql, q.c_str());
	coSqlRes = mysql_store_result(pSql);
	CheckForRows(*coSqlRes, 1);
	FetchRow(*coSqlRes, row);
	c.SetUser(atol(row[0]));
	c.SetKey(atol(row[1]));
	return 0;
}

// ModifyUser: perform modify user action (modify user information in the database)
// Return error code.
// Parameters:
//		CContext& c - current context
//		MYSQL* pSql - pointer to MySQL connection
//		const char* ppchParams[] - parameters to read from context (user information)
//		int param_nr - parameters number
//		const int errs[] - error list (errors codes)
//		int err_nr - errors number
int ModifyUser(CContext& c, MYSQL* pSql, const char* ppchParams[], int param_nr,
               const int errs[], int err_nr)
{
	char pchBuf[10000];
	c.SetModifyUser(true);
	string field_pref = "User", q, f, password;
	bool first = true;
	for (int i = 0; i < param_nr; i++)
	{
		if (!c.IsUserInfo(string(ppchParams[i])) || i == 3)
			continue;
		string s = c.UserInfo(string(ppchParams[i]));
		int slen = strlen(s.c_str()) > 5000 ? 5000 : strlen(s.c_str());
		mysql_escape_string(pchBuf, s.c_str(), slen);
		if (i < err_nr && s == "") return errs[i];
		if (s == "") continue;
		if (i == 4)
		{
			password = pchBuf;
			continue;
		}
		if (!first) f += ", ";
		f += string((i == 5 ? ppchParams[i - 1] : ppchParams[i])) + " = ";
		if (i == 5)
		{
			if (password != pchBuf)
				return UERR_PASSWORDS_DONT_MATCH;
			if (strlen(s.c_str()) < 6)
				return UERR_PASSWORD_TOO_SIMPLE;
			f += string("password(\"") + pchBuf + "\")";
		}
		else if (strncasecmp(ppchParams[i], "Pref", 4))
			f += string("\"") + pchBuf + "\"";
		else
			f += string("\"") + (s == "on" ? "Y" : "N") + "\"";
		if (first) first = false;
	}
	sprintf(pchBuf, "%ld", c.User());
	q = string("select * from Users where Id = \"") + pchBuf + "\"";
	SQLQuery(pSql, q.c_str());
	StoreResult(pSql, coSqlRes);
	if (mysql_num_rows(*coSqlRes) <= 0)
		return UERR_INVALID_USER;
	q = string("update Users set ") + f + " where Id = \"" + pchBuf + "\"";
	SQLQuery(pSql, q.c_str());
	return 0;
}

// DoSubscribe: perform subscribe action (subscribe user to a certain publication)
// Parameters:
//		const CURL* url - url object
//		CContext& c - current context
//		MYSQL* pSql - pointer to MySQL connection
int DoSubscribe(const CURL* url, CContext& c, MYSQL* pSql)
{
	if (c.SubsType() == ST_NONE)
		return SERR_TYPE_NOT_SPECIFIED;
	c.SetSubscribe(true);
	char pchBuf[2000];
	if (c.User() < 0)
		return SERR_USER_NOT_SPECIFIED;
	if (!c.IsReader())
		return SERR_USER_NOT_READER;
	if (c.Publication() < 0)
		return SERR_PUBL_NOT_SPECIFIED;
	string s;
	sprintf(pchBuf, "select TimeUnit, PayTime, UnitCost, Currency from Publications where "
	        "Id = %ld", c.Publication());
	SQLQuery(pSql, pchBuf);
	StoreResult(pSql, coSqlRes);
	CheckForRows(*coSqlRes, 1);
	FetchRow(*coSqlRes, row);
	const char* modifier = "";
	if (row[0][0] == 'D')
		modifier = "DAY";
	else if (row[0][0] == 'W')
		modifier = "WEEK";
	else if (row[0][0] == 'M')
		modifier = "MONTH";
	else if (row[0][0] == 'Y')
		modifier = "YEAR";
	else
		return SERR_UNIT_NOT_SPECIFIED;
	long int paid_time = atol(row[1]);
	double unit_cost = atof(row[2]);
	string currency = row[3];
	long int id_subscription;
	bool active = true;
	my_ulonglong rows = 0;
	sprintf(pchBuf, "select Id, Active, ToPay from Subscriptions where IdUser = %ld and "
	        "IdPublication = %ld", c.User(), c.Publication());
	SQLQuery(pSql, pchBuf);
	coSqlRes = mysql_store_result(pSql);
	char subs_type = c.SubsType() == ST_TRIAL ? 'T' : 'P';
	if (mysql_num_rows(*coSqlRes) > 0)
	{
		row = mysql_fetch_row(*coSqlRes);
		id_subscription = atol(row[0]);
		active = row[1][0] == 'Y';
		if (atof(row[2]) > 0)
			return SERR_SUBS_NOT_PAID;
		if (!active)
		{
			sprintf(pchBuf, "update Subscriptions set Active = 'Y', Type = '%c' were IdUser"
			        " = %ld and IdPublication = %ld", subs_type, c.User(), c.Publication());
			SQLQuery(pSql, pchBuf);
		}
	}
	else
	{
		sprintf(pchBuf, "insert into Subscriptions (IdUser, IdPublication, Active, Type) "
		        "values(%ld, %ld, 'Y', '%c')", c.User(), c.Publication(), subs_type);
		SQLQuery(pSql, pchBuf);
		sprintf(pchBuf, "select Id from Subscriptions where IdUser = %ld and "
		        "IdPublication = %ld", c.User(), c.Publication());
		SQLQuery(pSql, pchBuf);
		coSqlRes = mysql_store_result(pSql);
		CheckForRows(*coSqlRes, 1);
		row = mysql_fetch_row(*coSqlRes);
		id_subscription = atol(row[0]);
	}
	bool by_publication = false;
	double to_pay = 0;
	if ((s = url->getValue("by")) != "" && strcasecmp(s.c_str(), "publication") == 0)
	{
		by_publication = true;
		sprintf(pchBuf, "select Number from Issues where IdPublication = %ld and "
		        "Published = 'Y' and IdLanguage = %ld order by Number DESC limit 0, 1",
		        c.Publication(), c.Language());
		SQLQuery(pSql, pchBuf);
		coSqlRes = mysql_store_result(pSql);
		CheckForRows(*coSqlRes, 1);
		row = mysql_fetch_row(*coSqlRes);
		c.SetIssue(atol(row[0]));
		const char* sel_time = c.SubsType() == ST_TRIAL ? "TrialTime" : "PaidTime";
		sprintf(pchBuf, "select TO_DAYS(ADDDATE(now(), INTERVAL %s %s)) - TO_DAYS(now()), %s "
		        "from SubsDefTime, Users where IdPublication = %ld and "
		        "SubsDefTime.CountryCode = Users.CountryCode and Users.Id = %ld",
		        sel_time, modifier, sel_time, c.Publication(), c.User());
		SQLQuery(pSql, pchBuf);
		coSqlRes = mysql_store_result(pSql);
		if (mysql_num_rows(*coSqlRes) < 1) {
			sprintf(pchBuf, "select TO_DAYS(ADDDATE(now(), INTERVAL %s %s)) - TO_DAYS(now()), "
			        "%s from Publications where Id = %ld", sel_time, modifier, sel_time,
			        c.Publication());
			SQLQuery(pSql, pchBuf);
			coSqlRes = mysql_store_result(pSql);
			if (mysql_num_rows(*coSqlRes) < 1)
				return -1;
		}
		row = mysql_fetch_row(*coSqlRes);
		long int subs_days = atol(row[0]);
		long int time_units = atol(row[1]);
		if (c.SubsType() == ST_TRIAL)
			paid_time = subs_days;
		sprintf(pchBuf, "select Number from Sections where IdPublication = %ld and NrIssue "
		        "= %ld and IdLanguage = %ld", c.Publication(), c.Issue(), c.Language());
		SQLQuery(pSql, pchBuf);
		coSqlRes = mysql_store_result(pSql);
		CheckForRows(*coSqlRes, 1);
		while ((row = mysql_fetch_row(*coSqlRes)) != NULL)
		{
			sprintf(pchBuf, "replace into SubsSections set IdSubscription = %ld, "
			        "SectionNumber = %s, StartDate = now(), Days = %ld, PaidDays = %ld",
			         id_subscription, row[0], subs_days, paid_time);
			SQLQuery(pSql, pchBuf);
			to_pay += unit_cost * time_units;
		}
	}
	if ((s = url->getValue(P_CB_SUBS)) != "" && !by_publication)
		while (s != "")
		{
			long int section = atol(s.c_str());
			sprintf(pchBuf, "%s%s", P_TX_SUBS, s.c_str());
			if ((s = url->getValue(pchBuf)) == "")
			{
				s = url->getValue(P_CB_SUBS);
				continue;
			}
			long int time_units = atol(s.c_str());
			sprintf(pchBuf, "select TO_DAYS(ADDDATE(now(), INTERVAL %ld %s)) - TO_DAYS(now())",
			        time_units, modifier);
			SQLQuery(pSql, pchBuf);
			coSqlRes = mysql_store_result(pSql);
			CheckForRows(*coSqlRes, 1);
			row = mysql_fetch_row(*coSqlRes);
			long int req_days = atol(row[0]);
			if (c.SubsType() == ST_TRIAL)
				paid_time = req_days;
			sprintf(pchBuf, "select TO_DAYS(ADDDATE(StartDate, INTERVAL Days DAY)) - "
			        "TO_DAYS(now()) from SubsSections where IdSubscription = %ld and "
			        "SectionNumber = %ld", id_subscription, section);
			SQLQuery(pSql, pchBuf);
			coSqlRes = mysql_store_result(pSql);
			to_pay += unit_cost * time_units;
			if ((rows = mysql_num_rows(*coSqlRes)))
			{
				row = mysql_fetch_row(*coSqlRes);
				if (c.SubsType() == ST_TRIAL)
					paid_time = req_days;
				if (atol(row[0]) > 0)
					sprintf(pchBuf, "update SubsSections set Days = Days + %ld, PaidDays = "
					        "PaidDays + %ld where IdSubscription = %ld and SectionNumber = %ld",
					        req_days, paid_time, id_subscription, section);
				else
					sprintf(pchBuf, "update SubsSections set StartDate = now(), Days = %ld, "
					        "PaidDays = %ld where IdSubscription = %ld and SectionNumber = %ld",
					        req_days, paid_time, id_subscription, section);
			}
			else
				sprintf(pchBuf, "insert into SubsSections (IdSubscription, SectionNumber, "
				        "StartDate, Days, PaidDays) values(%ld, %ld, now(), %ld, %ld)",
				        id_subscription, section, req_days, paid_time);
			SQLQuery(pSql, pchBuf);
			s = url->getValue(P_CB_SUBS);
		}
	if (c.SubsType() != ST_TRIAL)
	{
		sprintf(pchBuf, "update Subscriptions set ToPay = ToPay + %f, Currency = '%s' "
		        "where Id = %ld", to_pay, currency.c_str(), id_subscription);
		SQLQuery(pSql, pchBuf);
	}
	return 0;
}

// SetReaderAccess: update current context: set reader access to publication sections
// according to user subscriptions.
// Parameters:
//		CContext& c - current context
//		MYSQL* pSql - pointer to MySQL connection
void SetReaderAccess(CContext& c, MYSQL* pSql)
{
	if (pSql == 0)
		return ;
	char pchBuf[300];
	sprintf(pchBuf, "select IdPublication, Id from Subscriptions where IdUser = %ld"
	        " and Active = \"Y\"", c.User());
	if (mysql_query(pSql, pchBuf))
		return ;
	MYSQL_RES* pSqlRes = mysql_store_result(pSql);
	if (pSqlRes == 0)
		return ;
	MYSQL_ROW row;
	while ((row = mysql_fetch_row(pSqlRes)))
	{
		long int id_publ = atol(row[0]), id_subs = atol(row[1]);
		sprintf(pchBuf, "select SectionNumber, (TO_DAYS(now())-TO_DAYS(StartDate)), "
		        "PaidDays from SubsSections where IdSubscription = %ld", id_subs);
		if (mysql_query(pSql, pchBuf))
			continue;
		MYSQL_RES *res2 = mysql_store_result(pSql);
		if (res2 == 0)
			continue;
		MYSQL_ROW row2;
		while ((row2 = mysql_fetch_row(res2)))
		{
			long int nr_section = atol(row2[0]);
			long int passed_days = atol(row2[1]);
			long int days = atol(row2[2]);
			if (passed_days <= days)
				c.SetSubs(id_publ, nr_section);
		}
		mysql_free_result(res2);
	}
	mysql_free_result(pSqlRes);
}

// Search: perform search action; search against the database for keywords retrieved from
// cgi environment
// Parameters:
//		CContext& c - current context
//		MYSQL* pSql - pointer to MySQL connection
//		const CURL* p_pcoURL - pointer to the URL object
int Search(CContext& c, MYSQL* pSql, const CURL* p_pcoURL)
{
	c.SetSearch(true);
	string s;
	if ((s = p_pcoURL->getValue("SearchKeywords")) == "")
		return SRERR_NO_KEYWORDS;
	ParseKeywords(s.c_str(), c);
	c.SetStrKeywords(s.c_str());
	if ((s = p_pcoURL->getValue("SearchMode")) != "" && strcasecmp(s.c_str(), "on") == 0)
		c.SetSearchAnd(true);
	int level = 0;
	if ((s = p_pcoURL->getValue("SearchLevel")) != "")
		level = atol(s.c_str());
	if (level < 0 || level > 2)
		return SRERR_INVALID_LEVEL;
	c.SetSearchLevel(level);
	return 0;
}

// ParseKeywords: read keywords from a string of keywords and add them to current context
// Parameters:
//		const char* s - string of keywords
//		CContext& c - current context
void ParseKeywords(const char* s, CContext& c)
{
	// " \t\n\r,./\\<>?:;\"'{}[]~`!%^&*()+=\\|"
	const char* p;
	const char* q;
	char tmp[256];
	int l;
	if (s)
	{
		for (q = s; *q; )
		{
			p = q;
			while (*q && !IsSeparator(*q))
				q++;
			l = q - p;
			if (l > 1)
			{
				strncpy(tmp, p, (l > 255 ? 255 : l));
				tmp[(l > 255 ? 255 : l)] = 0;
				c.SetKeyword(tmp);
			}
			else
			{
				while (*q && IsSeparator(*q))
					q++;
			}
		}
	}
}

// IsSeparator: return true if c character is separator
// Parameters:
//		char c - character to test
bool IsSeparator(char c)
{
	static const char separators[] = " \t\n\r,./\\<>?:;\"'{}[]~`!%^&*()+=\\|";
	for (int i = 0; separators[i]; i++)
		if (c == separators[i])
			return true;
	return false;
}
