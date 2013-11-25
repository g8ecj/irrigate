// Basic general functions

function getElementsByClass(searchClass,node,tag) {
	var classElements = new Array();
	if ( node == null )
		node = document;
	if ( tag == null )
		tag = '*';
	var els = node.getElementsByTagName(tag);
	var elsLen = els.length;
	var pattern = new RegExp('(^|\\\\s)'+searchClass+'(\\\\s|$)');
	for (i = 0, j = 0; i < elsLen; i++) {
		if ( pattern.test(els[i].className) ) {
			classElements[j] = els[i];
			j++;
		}
	}
	return classElements;
}

function getv(id)
{
   var d = get(id)
   if(!d)
      return 0
   if(d.value)
   {
      return parseInt(d.value)
   }
   return 0
}


function get(id)
{
   return document.getElementById(id)
}

function addLoadEvent(func) {
	var oldonload = window.onload;
	if (typeof window.onload != 'function') {
		window.onload = func;
	}
	else {
		window.onload = function() {
			oldonload();
			func();
		}
	}
}

function get_width(e)
{
   return parseInt(e.offsetWidth)
}

function get_height(e)
{
   return parseInt(e.offsetHeight)
}




function mouseX(evt) 
{
   if (evt.pageX) 
      return evt.pageX;
   else if (evt.clientX)
      return evt.clientX + (document.documentElement.scrollLeft ? document.documentElement.scrollLeft : document.body.scrollLeft);
   else 
      return null;
}

function mouseY(evt) 
{
   if (evt.pageY) 
      return evt.pageY;
   else if (evt.clientY)
      return evt.clientY + (document.documentElement.scrollTop ?    document.documentElement.scrollTop : document.body.scrollTop);
   else 
      return null;
}

function fade(div,opacity)
{
   if(!div.style)
      return;
    div.style.opacity = opacity;
    div.style.filter = "alpha(opacity=" + (opacity*100) + ")"; 
}

function show(div)
{
   div.style.display = '';
}

function hide(div)
{
   div.style.display = 'none';
}
