/*
    PowerDNS Versatile Database Driven Nameserver
    Copyright (C) 2002  PowerDNS.COM BV

    This program is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 2 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program; if not, write to the Free Software
    Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
*/
#include "utility.hh"
#include <fstream>
#include <iostream>
#include "logger.hh"
#include "arguments.hh"
#include "misc.hh"

#define L theL("pdns")
extern Logger &theL(const string &prefix);

const ArgvMap::param_t::const_iterator ArgvMap::begin()
{
  return params.begin();
}

const ArgvMap::param_t::const_iterator ArgvMap::end()
{
  return params.end();
}

string & ArgvMap::set(const string &var)
{
  return params[var];
}

bool ArgvMap::mustDo(const string &var)
{
  return ((*this)[var]!="no") && ((*this)[var]!="off");
}

vector<string>ArgvMap::list()
{
  vector<string> ret;
  for(map<string,string>::const_iterator i=params.begin();i!=params.end();++i)
    ret.push_back(i->first);
  return ret;
}

string ArgvMap::getHelp(const string &item)
{
  return helpmap[item];
}

string & ArgvMap::set(const string &var, const string &help)
{
  helpmap[var]=help;
  d_typeMap[var]="Parameter";
  return set(var);
}

void ArgvMap::setCmd(const string &var, const string &help)
{
  helpmap[var]=help;
  d_typeMap[var]="Command";
  set(var)="no";
}

string & ArgvMap::setSwitch(const string &var, const string &help)
{
  helpmap[var]=help;
  d_typeMap[var]="Switch";
  return set(var);
}


bool ArgvMap::contains(const string &var, const string &val)
{
  vector<string> parts;
  vector<string>::const_iterator i;
  
  stringtok( parts, params[var], ", \t" );
  for( i = parts.begin(); i != parts.end(); i++ ) {
    if( *i == val ) {
      return true;
    }
  }

  return false;
}


string ArgvMap::helpstring(string prefix)
{
  if(prefix=="no")
    prefix="";
  
  string help;
  
  for(map<string,string>::const_iterator i=helpmap.begin();
      i!=helpmap.end();
      i++)
    {
      if(!prefix.empty() && i->first.find(prefix)) // only print items with prefix
	continue;

      help+="  --";
      help+=i->first;
      
      string type=d_typeMap[i->first];

      if(type=="Parameter")
	help+="=...";
      else if(type=="Switch")
	{
	  help+=" | --"+i->first+"=yes";
	  help+=" | --"+i->first+"=no";
	}
      

      help+="\n\t";
      help+=i->second;
      help+="\n";

    }
  return help;
}

string ArgvMap::configstring()
{
  string help;
  
  help="# Autogenerated configuration file template\n";
  for(map<string,string>::const_iterator i=helpmap.begin();
      i!=helpmap.end();
      i++)
    {
      if(d_typeMap[i->first]=="Command")
	continue;

      help+="#################################\n";
      help+="# ";
      help+=i->first;
      help+="\t";
      help+=i->second;
      help+="\n#\n";
      help+="# "+i->first+"="+params[i->first]+"\n\n";

    }
  return help;
}


const string & ArgvMap::operator[](const string &arg)
{
  if(!parmIsset(arg))
    throw ArgException(string("Undefined but needed argument: '")+arg+"'");


  return params[arg];
}

int ArgvMap::asNum(const string &arg)
{
  if(!parmIsset(arg))
    throw ArgException(string("Undefined but needed argument: '")+arg+"'");

  return atoi(params[arg].c_str());
}

double ArgvMap::asDouble(const string &arg)
{
  if(!parmIsset(arg))
    throw ArgException(string("Undefined but needed argument: '")+arg+"'");

  return atof(params[arg].c_str());
}

ArgvMap::ArgvMap()
{

}

bool ArgvMap::parmIsset(const string &var)
{
  return (params.find(var)!=params.end());
}

void ArgvMap::parseOne(const string &arg, const string &parseOnly, bool lax)
{
  string var, val;
  unsigned int pos;

  if(!arg.find("--") &&(pos=arg.find("="))!=string::npos)  // this is a --port=25 case
    {
      var=arg.substr(2,pos-2);
      val=arg.substr(pos+1);
    }
  else if(!arg.find("--") && (arg.find("=")==string::npos))  // this is a --daemon case
    { 
      var=arg.substr(2);
      val="";
    }
  else if(arg[0]=='-')
    {
      var=arg.substr(1);
      val="";
    }
  else { // command 
    d_cmds.push_back(arg);
  }

  if(var!="" && (parseOnly.empty() || var==parseOnly)) {

    pos=val.find_first_not_of(" \t");  // strip leading whitespace
    if(pos && pos!=string::npos) 
      val=val.substr(pos);

    if(parmIsset(var))
      params[var]=val;
    else
      if(!lax)
	throw ArgException("Trying to set unexisting parameter '"+var+"'");
  }
}

const vector<string>&ArgvMap::getCommands()
{
  return d_cmds;
}

void ArgvMap::parse(int &argc, char **argv, bool lax)
{
  for(int n=1;n<argc;n++) {
    parseOne(argv[n],"",lax);
  }
}

void ArgvMap::preParse(int &argc, char **argv, const string &arg)
{
  for(int n=1;n<argc;n++) {
    string varval=argv[n];
    if(!varval.find("--"+arg))
      parseOne(argv[n]);
  }
}

bool ArgvMap::preParseFile(const char *fname, const string &arg)
{
  ifstream f(fname);
  if(!f) 
    return false;
  
  string line;
  string pline;
  unsigned int pos;

  while(getline(f,pline)) {
    chomp(pline,"\t\r\n");
    if(pline[pline.size()-1]=='\\') {
      line+=pline.substr(0,pline.length()-1);
      continue;
    }
    else 
      line+=pline;
    

    // strip everything after a #
    if((pos=line.find("#"))!=string::npos)
      line=line.substr(0,pos);

    // strip trailing spaces
    chomp(line," ");
    
    // strip leading spaces
    if((pos=line.find_first_not_of(" \t\r\n"))!=string::npos)
      line=line.substr(pos);
    
    // gpgsql-basic-query=sdfsdfs dfsdfsdf sdfsdfsfd

    parseOne(string("--")+line, arg);      
    line="";
  }
  
  return true;
}


bool ArgvMap::file(const char *fname, bool lax)
{
  ifstream f(fname);
  if(!f) {
    //    L<<"Tried file '"<<fname<<"' for configuration"<<endl;
    return false;
  }
  if(!lax)
    L<<"Opened file '"<<fname<<"' for configuration"<<endl;
  
  string line;
  string pline;
  unsigned int pos;
  while(getline(f,pline)) {
    chomp(pline,"\t\r\n");

    if(pline[pline.size()-1]=='\\') {
      line+=pline.substr(0,pline.length()-1);

      continue;
    }
    else 
      line+=pline;

    // strip everything after a #
    if((pos=line.find("#"))!=string::npos)
      line=line.substr(0,pos);

    // strip trailing spaces
    chomp(line," ");

    
    // strip leading spaces
    if((pos=line.find_first_not_of(" \t\r\n"))!=string::npos)
      line=line.substr(pos);
    
    
    parseOne(string("--")+line,"",lax);      
    line="";
  }
  
  return true;
}
