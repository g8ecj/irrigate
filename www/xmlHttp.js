// This is a very simple json send / receive thype thing.
// Will only do one object at a time!

function submit_object_as_JSON(obj,url,callback,id)
{
   submit_http(JSON.stringify(obj),url,callback,id);
}

function submit_object_as_query_string(obj,url,callback,id)
{
   // Will not convert objects in the object
   var query_count=0
   var query_string=""
   for(var i in obj)
   {
      query_count++
      if(query_count>1)
         query_string+="&"
      query_string+= i+"="+obj[i]
   }
   submit_http(null,url+"?"+query_string,callback,id);
}



var xmlHttpMsgs = []
var xmlState = 0

var xmlHttp
var xmlHttp_observer
var xmlID

function submit_http(str, url, observer,id)
{
   xmlHttpMsgs.push( { str: str, url: url, observer: observer, id:id })
   send_http()
}



function send_http()
{
   var hq = get("http_q")
   if(hq)
      hq.innerHTML = xmlHttpMsgs.length
   if(xmlHttpMsgs.length == 0)
      return;
   if (xmlState != 0)
   {
//      alert("xml queued: "+xmlHttpMsgs.length)
      return;
   }

  var x = xmlHttpMsgs.shift()

  xmlHttp_observer = x.observer
  xmlHttp=GetXmlHttpObject();
  xmlID = x.id
  if (xmlHttp==null)
  {
     alert ("Your browser does not support AJAX!");
     return;
  }


  if(xml_show)
     alert("submit_http:\nurl='"+x.url+"'\nstr='"+x.str+"'\n")

   xmlState = setTimeout("xml_http_failed()",1500)


//  document.getElementById("result").innerHTML = "Processing..."

//   alert("get_http: url='"+url+"' str='"+str+"' + callback="+observer)




  xmlHttp.onreadystatechange=state_changed;

  xmlHttp.open("POST", x.url, true)
  xmlHttp.setRequestHeader("Content-type", "text/plain")
  xmlHttp.send(x.str)
}

var xml_show = false
function state_changed()
{
//   alert("xmlHTTP: State_changed: "+xmlHttp.readyState+"\n="+xmlHttp.responseText)
  if (xmlHttp.readyState==4)
  {
//      alert(xmlHttp_observer+"\n="+xmlHttp.responseText)
      if(xml_show)
      {
         alert("xmlHttp.responseText=\n'"+xmlHttp.responseText+"'")
         xml_show=0
      }

      if(xmlHttp.responseText.search("404 Simon") != -1)
      {
//         alert("404 error"+xmlHttp_observer)
         var obj ={ error: "ERROR 404 Not Found" }
      }
      else if(xmlHttp.responseText.substr(0,1) == "{")
      {
         var obj = JSON.parse(xmlHttp.responseText)
         if(!obj)
            obj = {}
      }
      else
         var obj ={}

      xmlHttp_observer(obj,xmlID);

      clearTimeout(xmlState)

      xmlState = 0

      send_http()
  }
}

function GetXmlHttpObject()
{
var xmlHttp=null;
try
 {
 // Firefox, Opera 8.0+, Safari
 xmlHttp=new XMLHttpRequest();
 }
catch (e)
 {
 // Internet Explorer
 try
   {
   xmlHttp=new ActiveXObject("Msxml2.XMLHTTP");
   }
 catch (e)
   {
   xmlHttp=new ActiveXObject("Microsoft.XMLHTTP");
   }
 }
return xmlHttp;
}


function xml_http_failed()
{
//   alert("xml_http_failed")

   // It failed, try the next one:
   var obj ={ error: "No reply" }
   xmlHttp_observer(obj,xmlID);
   xmlState = 0
   send_http()
}
