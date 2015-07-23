function httpPost(url, params, cb) {
	var p = '';
	for (var param in params) {
		if (p !== '') {
			p += "&";
		}
		p += param + "=" + params[param];
	}
	var req = new XMLHttpRequest();
	req.open('POST', url, true);
	req.setRequestHeader("Content-type", "application/x-www-form-urlencoded");
	req.setRequestHeader("Content-length", p.length);
	req.setRequestHeader("Connection", "close");

	req.onreadystatechange = function() {
		if(req.readyState == 4 && req.status == 200) {
			cb(req.responseText);
		}
	};
	req.send(p);
}

function parseQuery(query) {
	var params = {};
	var vars = query.split('&');
	for(var i = 0; i < vars.length; ++i) {
		var pair = vars[i].split('=');
		params[pair[0]] = pair[1];
	}
	return params;
}

var pebbleSendQueue = {
	queue: [],
	queueFull: false,
	send: function(msg) {
		if (this.queueFull) {
			this.queue.push(msg);
			return;
		}
		this.queueFull = true;
		this._doSend(msg);
	},
	_sendDone: function(e) {
		if (this.queue.length === 0) {
			this.queueFull = false;
			return;
		}
		var msg = pebbleSendQueue.queue.splice(0,1)[0];
		this._doSend(msg);
	},
	_sendFailed: function(e) {
		var msg = pebbleSendQueue.queue.splice(0,1)[0];
		this._doSend(msg);
	},
	_doSend: function(msg) {
		this.inQueue = msg;
		Pebble.sendAppMessage(msg, function(e) { pebbleSendQueue._sendDone(e); }, function(e) { pebbleSendQueue._sendFailed(e); });
	}
};

var dispatcher = {
	publicKey: 'PUT YOUR PUBLIC KEY HERE',
	privateKey: 'PUT YOU PRIVATE KEY HERE',
	requestTokenUrl: 'https://api.telldus.com/oauth/requestToken',
	authorizeUrl: 'https://api.telldus.com/oauth/authorize',
	accessTokenUrl: 'https://api.telldus.com/oauth/accessToken',
	token: '',
  tokenSecret: '', 
	init: function() {
	this.token = window.localStorage.getItem('token');
	this.tokenSecret =  window.localStorage.getItem('tokenSecret');
	if (this.token === '' || this.token === null) {
			this.authenticate();
		} else {
			getDevices();
		}
	},
	authenticate: function() {
		this.token = '';  // Make sure these are empty
		this.tokenSecret = '';
		pebbleSendQueue.send({ "module": "auth", "action": "clear" });
		var authorizeUrl = this.authorizeUrl;
		this._doOAuth(this.requestTokenUrl, { oauth_callback: 'pebblejs://close#' }, function(query) {
			var params = parseQuery(query);
			if ('oauth_token' in params && 'oauth_token_secret' in params) {
				dispatcher.token = params.oauth_token;
				dispatcher.tokenSecret = params.oauth_token_secret;
				var url = authorizeUrl + '?oauth_token=' + params.oauth_token;
				pebbleSendQueue.send({ "module": "auth", "action": "request" });
				Pebble.addEventListener("showConfiguration", function(e) {
					Pebble.openURL(url);
				});
				return;
			}
			Pebble.showSimpleNotificationOnPebble("Error", "An error occured while trying to authorize Pebble with your Telldus Live! account");
		});
		return;
	},
	getAccessToken: function(cb) {
		pebbleSendQueue.send({ "module": "auth", "action": "authenticating" });
		this._doOAuth(this.accessTokenUrl, {}, function(response) {
			var params = parseQuery(response);
			if ('oauth_token' in params && 'oauth_token_secret' in params) {
				dispatcher.token = params.oauth_token;
				dispatcher.tokenSecret = params.oauth_token_secret;
				window.localStorage.setItem('token', params.oauth_token);
				window.localStorage.setItem('tokenSecret', params.oauth_token_secret);
				pebbleSendQueue.send({ "module": "auth", "action": "log" });
				Pebble.showSimpleNotificationOnPebble("Telldus Live!", "Pebble was successfully paired with your Telldus Live! account");
				cb();
			}
		});
	},
	doCall: function(endpoint, params, cb) {
		this._doOAuth('https://api.telldus.com/json/' + endpoint, params, function(result) {
			var response = JSON.parse(result);
			cb(response);
		});
	},
	_setupOAuth: function(url, params) {
		var message = {
			action: url,
			method: 'POST',
			parameters: [
				['oauth_consumer_key', this.publicKey],
				['oauth_token', this.token],
				['oauth_signature_method', 'HMAC-SHA1'],
				['oauth_timestamp', ''],
				['oauth_nonce', ''],
				['oauth_signature', ''],
			]
		};
		for(var p in params) {
			message.parameters.push([p, params[p]]);
		}
		OAuth.setTimestampAndNonce(message);
		OAuth.SignatureMethod.sign(message, {
			consumerSecret: this.privateKey,
			tokenSecret: this.tokenSecret
		});
		return OAuth.getParameterMap(message.parameters);
	},
	_doOAuth: function(url, params, cb) {
		httpPost(url, this._setupOAuth(url, params), cb);
	}
};

var devices = {};
var sensors = {};
var clients = {};

function getDevices() {
  
dispatcher.doCall('clients/list', '' , function(r) {
	console.log("clients");

	clients = r.client;
	for(var i in clients) {
		var name;
		if (clients[i].name)
			name = clients[i].name;
		else 
			name = 'unknown';
		if (name.length > 16) {
			name = name.substr(0,16);
		} 
		var online = 0;
		var temp;
		if (clients[i].online)
			online = clients[i].online;
			console.log(online);

		pebbleSendQueue.send({
			module: "client",
			action: "info",
			name: name,
			temp: online
		});
		//pebbleSendQueue.send({ "module": "auth", "action": "log" });

	}
});
	
	
dispatcher.doCall('sensors/list', { includeValues: 1}, function(r) {
	console.log("sensor");
	sensors = r.sensor;
	for(var i in sensors) {
		var name;
		if (sensors[i].name)
			name = sensors[i].name;
		else 
			name = 'unknown';
		if (name.length > 16) {
			name = name.substr(0,16);
		} 
		
		var temp = sensors[i].temp;
		
		var humidity = "";
		if(sensors[i].humidity) {
			humidity = sensors[i].humidity;
		}
		
		var battery = "254";
		if(sensors[i].battery) {
			battery = sensors[i].battery;
		}
		
		pebbleSendQueue.send({
			module: "sensor",
			action: "info",
			name: name,
			temp: temp,
			hum: humidity,
			id: parseInt(i),
			battery: parseInt(battery)
		});
	}
	pebbleSendQueue.send({ "module": "auth", "action": "done" });
});
  
	function is_int(value){ 
  if((parseFloat(value) == parseInt(value)) && !isNaN(value)){
      return true;
  } else { 
      return false;
  } 
}


dispatcher.doCall('devices/list', {supportedMethods: 1023}, function(r) {
	console.log("device");
	devices = r.device;
	for(var i in devices) {
		var type = devices[i].type;
	
		var dimvalue = 0;
		
		if (is_int(devices[i].statevalue)) {
			dimvalue = parseInt(devices[i].statevalue);
		}
		
		if (type == "group") {
			dimvalue = 0;
			type = 0;
		}
		else
			type = 1;
		
		var name;
		if (devices[i].name) 
			name = devices[i].name;
		else 
			name = 'unknown';
		
		if (name.length > 16) {
			name = name.substr(0,16);
		} 
		
		pebbleSendQueue.send({
			module: "device",
			action: "info",
			name: name,
			state: parseInt(devices[i].state),
			methods: parseInt(devices[i].methods),
			dimvalue: dimvalue,
			id: parseInt(i),
			type:  parseInt(type)
		});
	}
	pebbleSendQueue.send({ "module": "auth", "action": "done" });
});
}

Pebble.addEventListener("ready", function(e) {
	if (window.localStorage.getItem('myversion') != '1.0') {	
		window.localStorage.setItem('myversion', '1.1');
		window.localStorage.setItem('token', '');
		window.localStorage.setItem('tokenSecret', '');
	}
	console.log("Ready, with callback");
	dispatcher.init();
});

Pebble.addEventListener("appmessage", function(e) {
	var msg = e.payload;
	var id;
	var method;
	var name;
	var dimvalue;
	if (!('module' in msg) || !('action' in msg)) {
		return;
	}
	if (msg.module == 'device') {
		if (msg.action == 'select') {
			id = msg.id;
			if (!(id in devices)) {
				return;
			}
			name = devices[id].name;
			if (devices[id].state == parseInt(2)) {
				method = 1;
			} else {
				method = 2;
			}
			dimvalue = 0;
			if (msg.methods == 16) {
					method = 16;
					dimvalue = msg.dimvalue.toString();
			}
			dispatcher.doCall('device/command', {id: devices[id].id, method: method, value: dimvalue}, function(r) {
				if (dimvalue == "0" && method == 16) method = 2;
				devices[id].state = method;
			}
      );
			pebbleSendQueue.send({
				module: "device",
				action: "info",
				name: devices[id].name,
				state: parseInt(method),
				methods: parseInt(devices[id].methods),
				dimvalue: parseInt(devices[id].statevalue),
				id: parseInt(id),
				type:  parseInt(0)
			});
		}
	}
});

//Pebble.addEventListener("showConfiguration", function(e) {
//	console.log("Show config");
	//dispatcher.authenticate();
//});
												
Pebble.addEventListener("webviewclosed", function(e) {
	console.log("Configuration done!");
	var query = e.response;
//	console.log(e.response);
	if (query.substr(0,1) == '?') {
		query = query.substr(1, query.length-1);
	}
	var params = parseQuery(query);
	dispatcher.getAccessToken(function() {
		getDevices();
	});
});


/*
* Copyright 2008 Netflix, Inc.
*
* Licensed under the Apache License, Version 2.0 (the "License");
* you may not use this file except in compliance with the License.
* You may obtain a copy of the License at
*
*     http://www.apache.org/licenses/LICENSE-2.0
*
* Unless required by applicable law or agreed to in writing, software
* distributed under the License is distributed on an "AS IS" BASIS,
* WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
* See the License for the specific language governing permissions and
* limitations under the License.
*/

/* Here's some JavaScript software for implementing OAuth.

This isn't as useful as you might hope.  OAuth is based around
allowing tools and websites to talk to each other.  However,
JavaScript running in web browsers is hampered by security
restrictions that prevent code running on one website from
accessing data stored or served on another.

Before you start hacking, make sure you understand the limitations
posed by cross-domain XMLHttpRequest.

On the bright side, some platforms use JavaScript as their
language, but enable the programmer to access other web sites.
Examples include Google Gadgets, and Microsoft Vista Sidebar.
For those platforms, this library should come in handy.
*/

// The HMAC-SHA1 signature method calls b64_hmac_sha1, defined by
// http://pajhome.org.uk/crypt/md5/sha1.js

/* An OAuth message is represented as an object like this:
{method: "GET", action: "http://server.com/path", parameters: ...}

The parameters may be either a map {name: value, name2: value2}
or an Array of name-value pairs [[name, value], [name2, value2]].
The latter representation is more powerful: it supports parameters
in a specific sequence, or several parameters with the same name;
for example [["a", 1], ["b", 2], ["a", 3]].

Parameter names and values are NOT percent-encoded in an object.
They must be encoded before transmission and decoded after reception.
For example, this message object:
{method: "GET", action: "http://server/path", parameters: {p: "x y"}}
... can be transmitted as an HTTP request that begins:
GET /path?p=x%20y HTTP/1.0
(This isn't a valid OAuth request, since it lacks a signature etc.)
Note that the object "x y" is transmitted as x%20y.  To encode
parameters, you can call OAuth.addToURL, OAuth.formEncode or
OAuth.getAuthorization.

This message object model harmonizes with the browser object model for
input elements of an form, whose value property isn't percent encoded.
The browser encodes each value before transmitting it. For example,
see consumer.setInputs in example/consumer.js.
*/

/* This script needs to know what time it is. By default, it uses the local
clock (new Date), which is apt to be inaccurate in browsers. To do
better, you can load this script from a URL whose query string contains
an oauth_timestamp parameter, whose value is a current Unix timestamp.
For example, when generating the enclosing document using PHP:

<script src="oauth.js?oauth_timestamp=<?=time()?>" ...

Another option is to call OAuth.correctTimestamp with a Unix timestamp.
*/

var OAuth; if (OAuth == null) OAuth = {};

OAuth.setProperties = function setProperties(into, from) {
if (into != null && from != null) {
	for (var key in from) {
		into[key] = from[key];
	}
}
return into;
}

OAuth.setProperties(OAuth, // utility functions
{
	percentEncode: function percentEncode(s) {
		if (s == null) {
			return "";
		}
		if (s instanceof Array) {
			var e = "";
			for (var i = 0; i < s.length; ++s) {
				if (e != "") e += '&';
				e += OAuth.percentEncode(s[i]);
			}
			return e;
		}
		s = encodeURIComponent(s);
		// Now replace the values which encodeURIComponent doesn't do
		// encodeURIComponent ignores: - _ . ! ~ * ' ( )
		// OAuth dictates the only ones you can ignore are: - _ . ~
		// Source: http://developer.mozilla.org/en/docs/Core_JavaScript_1.5_Reference:Global_Functions:encodeURIComponent
		s = s.replace(/\!/g, "%21");
		s = s.replace(/\*/g, "%2A");
		s = s.replace(/\'/g, "%27");
		s = s.replace(/\(/g, "%28");
		s = s.replace(/\)/g, "%29");
		return s;
	}
	,
	decodePercent: function decodePercent(s) {
		if (s != null) {
			// Handle application/x-www-form-urlencoded, which is defined by
			// http://www.w3.org/TR/html4/interact/forms.html#h-17.13.4.1
			s = s.replace(/\+/g, " ");
		}
		return decodeURIComponent(s);
	}
	,
	/** Convert the given parameters to an Array of name-value pairs. */
	getParameterList: function getParameterList(parameters) {
		if (parameters == null) {
			return [];
		}
		if (typeof parameters != "object") {
			return OAuth.decodeForm(parameters + "");
		}
		if (parameters instanceof Array) {
			return parameters;
		}
		var list = [];
		for (var p in parameters) {
			list.push([p, parameters[p]]);
		}
		return list;
	}
	,
	/** Convert the given parameters to a map from name to value. */
	getParameterMap: function getParameterMap(parameters) {
		if (parameters == null) {
			return {};
		}
		if (typeof parameters != "object") {
			return OAuth.getParameterMap(OAuth.decodeForm(parameters + ""));
		}
		if (parameters instanceof Array) {
			var map = {};
			for (var p = 0; p < parameters.length; ++p) {
				var key = parameters[p][0];
				if (map[key] === undefined) { // first value wins
					map[key] = parameters[p][1];
				}
			}
			return map;
		}
		return parameters;
	}
	,
	getParameter: function getParameter(parameters, name) {
		if (parameters instanceof Array) {
			for (var p = 0; p < parameters.length; ++p) {
				if (parameters[p][0] == name) {
					return parameters[p][1]; // first value wins
				}
			}
		} else {
			return OAuth.getParameterMap(parameters)[name];
		}
		return null;
	}
	,
	formEncode: function formEncode(parameters) {
		var form = "";
		var list = OAuth.getParameterList(parameters);
		for (var p = 0; p < list.length; ++p) {
			var value = list[p][1];
			if (value == null) value = "";
			if (form != "") form += '&';
			form += OAuth.percentEncode(list[p][0])
			+'='+ OAuth.percentEncode(value);
		}
		return form;
	}
	,
	decodeForm: function decodeForm(form) {
		var list = [];
		var nvps = form.split('&');
		for (var n = 0; n < nvps.length; ++n) {
			var nvp = nvps[n];
			if (nvp == "") {
				continue;
			}
			var equals = nvp.indexOf('=');
			var name;
			var value;
			if (equals < 0) {
				name = OAuth.decodePercent(nvp);
				value = null;
			} else {
				name = OAuth.decodePercent(nvp.substring(0, equals));
				value = OAuth.decodePercent(nvp.substring(equals + 1));
			}
			list.push([name, value]);
		}
		return list;
	}
	,
	setParameter: function setParameter(message, name, value) {
		var parameters = message.parameters;
		if (parameters instanceof Array) {
			for (var p = 0; p < parameters.length; ++p) {
				if (parameters[p][0] == name) {
					if (value === undefined) {
						parameters.splice(p, 1);
					} else {
						parameters[p][1] = value;
						value = undefined;
					}
				}
			}
			if (value !== undefined) {
				parameters.push([name, value]);
			}
		} else {
			parameters = OAuth.getParameterMap(parameters);
			parameters[name] = value;
			message.parameters = parameters;
		}
	}
	,
	setParameters: function setParameters(message, parameters) {
		var list = OAuth.getParameterList(parameters);
		for (var i = 0; i < list.length; ++i) {
			OAuth.setParameter(message, list[i][0], list[i][1]);
		}
	}
	,
	/** Fill in parameters to help construct a request message.
	This function doesn't fill in every parameter.
	The accessor object should be like:
	{consumerKey:'foo', consumerSecret:'bar', accessorSecret:'nurn', token:'krelm', tokenSecret:'blah'}
	The accessorSecret property is optional.
	*/
	completeRequest: function completeRequest(message, accessor) {
		if (message.method == null) {
			message.method = "GET";
		}
		var map = OAuth.getParameterMap(message.parameters);
		if (map.oauth_consumer_key == null) {
			OAuth.setParameter(message, "oauth_consumer_key", accessor.consumerKey || "");
		}
		if (map.oauth_token == null && accessor.token != null) {
			OAuth.setParameter(message, "oauth_token", accessor.token);
		}
		if (map.oauth_version == null) {
			OAuth.setParameter(message, "oauth_version", "1.0");
		}
		if (map.oauth_timestamp == null) {
			OAuth.setParameter(message, "oauth_timestamp", OAuth.timestamp());
		}
		if (map.oauth_nonce == null) {
			OAuth.setParameter(message, "oauth_nonce", OAuth.nonce(6));
		}
		OAuth.SignatureMethod.sign(message, accessor);
	}
	,
	setTimestampAndNonce: function setTimestampAndNonce(message) {
		OAuth.setParameter(message, "oauth_timestamp", OAuth.timestamp());
		OAuth.setParameter(message, "oauth_nonce", OAuth.nonce(6));
	}
	,
	addToURL: function addToURL(url, parameters) {
		newURL = url;
		if (parameters != null) {
			var toAdd = OAuth.formEncode(parameters);
			if (toAdd.length > 0) {
				var q = url.indexOf('?');
				if (q < 0) newURL += '?';
				else       newURL += '&';
				newURL += toAdd;
			}
		}
		return newURL;
	}
	,
	/** Construct the value of the Authorization header for an HTTP request. */
	getAuthorizationHeader: function getAuthorizationHeader(realm, parameters) {
		var header = 'OAuth realm="' + OAuth.percentEncode(realm) + '"';
		var list = OAuth.getParameterList(parameters);
		for (var p = 0; p < list.length; ++p) {
			var parameter = list[p];
			var name = parameter[0];
			if (name.indexOf("oauth_") == 0) {
				header += ',' + OAuth.percentEncode(name) + '="' + OAuth.percentEncode(parameter[1]) + '"';
			}
		}
		return header;
	}
	,
	/** Correct the time using a parameter from the URL from which the last script was loaded. */
	correctTimestampFromSrc: function correctTimestampFromSrc(parameterName) {
		parameterName = parameterName || "oauth_timestamp";
		var scripts = document.getElementsByTagName('script');
		if (scripts == null || !scripts.length) return;
		var src = scripts[scripts.length-1].src;
		if (!src) return;
		var q = src.indexOf("?");
		if (q < 0) return;
		parameters = OAuth.getParameterMap(OAuth.decodeForm(src.substring(q+1)));
		var t = parameters[parameterName];
		if (t == null) return;
		OAuth.correctTimestamp(t);
	}
	,
	/** Generate timestamps starting with the given value. */
	correctTimestamp: function correctTimestamp(timestamp) {
		OAuth.timeCorrectionMsec = (timestamp * 1000) - (new Date()).getTime();
	}
	,
	/** The difference between the correct time and my clock. */
	timeCorrectionMsec: 0
	,
	timestamp: function timestamp() {
		var t = (new Date()).getTime() + OAuth.timeCorrectionMsec;
		return Math.floor(t / 1000);
	}
	,
	nonce: function nonce(length) {
		var chars = OAuth.nonce.CHARS;
		var result = "";
		for (var i = 0; i < length; ++i) {
			var rnum = Math.floor(Math.random() * chars.length);
			result += chars.substring(rnum, rnum+1);
		}
		return result;
	}
});

OAuth.nonce.CHARS = "0123456789ABCDEFGHIJKLMNOPQRSTUVWXTZabcdefghiklmnopqrstuvwxyz";

/** Define a constructor function,
without causing trouble to anyone who was using it as a namespace.
That is, if parent[name] already existed and had properties,
copy those properties into the new constructor.
*/
OAuth.declareClass = function declareClass(parent, name, newConstructor) {
	var previous = parent[name];
	parent[name] = newConstructor;
	if (newConstructor != null && previous != null) {
		for (var key in previous) {
			if (key != "prototype") {
				newConstructor[key] = previous[key];
			}
		}
	}
	return newConstructor;
}

/** An abstract algorithm for signing messages. */
OAuth.declareClass(OAuth, "SignatureMethod", function OAuthSignatureMethod(){});

OAuth.setProperties(OAuth.SignatureMethod.prototype, // instance members
	{
		/** Add a signature to the message. */
		sign: function sign(message) {
			var baseString = OAuth.SignatureMethod.getBaseString(message);
			var signature = this.getSignature(baseString);
			OAuth.setParameter(message, "oauth_signature", signature);
			return signature; // just in case someone's interested
		}
		,
		/** Set the key string for signing. */
		initialize: function initialize(name, accessor) {
			var consumerSecret;
			if (accessor.accessorSecret != null
				&& name.length > 9
				&& name.substring(name.length-9) == "-Accessor")
				{
					consumerSecret = accessor.accessorSecret;
				} else {
					consumerSecret = accessor.consumerSecret;
				}
				this.key = OAuth.percentEncode(consumerSecret)
				+"&"+ OAuth.percentEncode(accessor.tokenSecret);
			}
		});

		/* SignatureMethod expects an accessor object to be like this:
		{tokenSecret: "lakjsdflkj...", consumerSecret: "QOUEWRI..", accessorSecret: "xcmvzc..."}
		The accessorSecret property is optional.
		*/
		// Class members:
		OAuth.setProperties(OAuth.SignatureMethod, // class members
			{
				sign: function sign(message, accessor) {
					var name = OAuth.getParameterMap(message.parameters).oauth_signature_method;
					if (name == null || name == "") {
						name = "HMAC-SHA1";
						OAuth.setParameter(message, "oauth_signature_method", name);
					}
					OAuth.SignatureMethod.newMethod(name, accessor).sign(message);
				}
				,
				/** Instantiate a SignatureMethod for the given method name. */
				newMethod: function newMethod(name, accessor) {
					var impl = OAuth.SignatureMethod.REGISTERED[name];
					if (impl != null) {
						var method = new impl();
						method.initialize(name, accessor);
						return method;
					}
					var err = new Error("signature_method_rejected");
					var acceptable = "";
					for (var r in OAuth.SignatureMethod.REGISTERED) {
						if (acceptable != "") acceptable += '&';
						acceptable += OAuth.percentEncode(r);
					}
					err.oauth_acceptable_signature_methods = acceptable;
					throw err;
				}
				,
				/** A map from signature method name to constructor. */
				REGISTERED : {}
				,
				/** Subsequently, the given constructor will be used for the named methods.
				The constructor will be called with no parameters.
				The resulting object should usually implement getSignature(baseString).
				You can easily define such a constructor by calling makeSubclass, below.
				*/
				registerMethodClass: function registerMethodClass(names, classConstructor) {
					for (var n = 0; n < names.length; ++n) {
						OAuth.SignatureMethod.REGISTERED[names[n]] = classConstructor;
					}
				}
				,
				/** Create a subclass of OAuth.SignatureMethod, with the given getSignature function. */
				makeSubclass: function makeSubclass(getSignatureFunction) {
					var superClass = OAuth.SignatureMethod;
					var subClass = function() {
						superClass.call(this);
					};
					subClass.prototype = new superClass();
					// Delete instance variables from prototype:
					// delete subclass.prototype... There aren't any.
					subClass.prototype.getSignature = getSignatureFunction;
					subClass.prototype.constructor = subClass;
					return subClass;
				}
				,
				getBaseString: function getBaseString(message) {
					var URL = message.action;
					var q = URL.indexOf('?');
					var parameters;
					if (q < 0) {
						parameters = message.parameters;
					} else {
						// Combine the URL query string with the other parameters:
						parameters = OAuth.decodeForm(URL.substring(q + 1));
						var toAdd = OAuth.getParameterList(message.parameters);
						for (var a = 0; a < toAdd.length; ++a) {
							parameters.push(toAdd[a]);
						}
					}
					return OAuth.percentEncode(message.method.toUpperCase())
					+'&'+ OAuth.percentEncode(OAuth.SignatureMethod.normalizeUrl(URL))
					+'&'+ OAuth.percentEncode(OAuth.SignatureMethod.normalizeParameters(parameters));
				}
				,
				normalizeUrl: function normalizeUrl(url) {
					var uri = OAuth.SignatureMethod.parseUri(url);
					var scheme = uri.protocol.toLowerCase();
					var authority = uri.authority.toLowerCase();
					var dropPort = (scheme == "http" && uri.port == 80)
					|| (scheme == "https" && uri.port == 443);
					if (dropPort) {
						// find the last : in the authority
						var index = authority.lastIndexOf(":");
						if (index >= 0) {
							authority = authority.substring(0, index);
						}
					}
					var path = uri.path;
					if (!path) {
						path = "/"; // conforms to RFC 2616 section 3.2.2
					}
					// we know that there is no query and no fragment here.
					return scheme + "://" + authority + path;
				}
				,
				parseUri: function parseUri (str) {
					/* This function was adapted from parseUri 1.2.1
					http://stevenlevithan.com/demo/parseuri/js/assets/parseuri.js
					*/
					var o = {key: ["source","protocol","authority","userInfo","user","password","host","port","relative","path","directory","file","query","anchor"],
					parser: {strict: /^(?:([^:\/?#]+):)?(?:\/\/((?:(([^:@\/]*):?([^:@\/]*))?@)?([^:\/?#]*)(?::(\d*))?))?((((?:[^?#\/]*\/)*)([^?#]*))(?:\?([^#]*))?(?:#(.*))?)/ }};
					var m = o.parser.strict.exec(str);
					var uri = {};
					var i = 14;
					while (i--) uri[o.key[i]] = m[i] || "";
					return uri;
				}
				,
				normalizeParameters: function normalizeParameters(parameters) {
					if (parameters == null) {
						return "";
					}
					var list = OAuth.getParameterList(parameters);
					var sortable = [];
					for (var p = 0; p < list.length; ++p) {
						var nvp = list[p];
						if (nvp[0] != "oauth_signature") {
							sortable.push([ OAuth.percentEncode(nvp[0])
							+ " " // because it comes before any character that can appear in a percentEncoded string.
							+ OAuth.percentEncode(nvp[1])
							, nvp]);
						}
					}
					sortable.sort(function(a,b) {
						if (a[0] < b[0]) return  -1;
						if (a[0] > b[0]) return 1;
						return 0;
					});
					var sorted = [];
					for (var s = 0; s < sortable.length; ++s) {
						sorted.push(sortable[s][1]);
					}
					return OAuth.formEncode(sorted);
				}
			});

			OAuth.SignatureMethod.registerMethodClass(["PLAINTEXT", "PLAINTEXT-Accessor"],
			OAuth.SignatureMethod.makeSubclass(
				function getSignature(baseString) {
					return this.key;
				}
			));

			OAuth.SignatureMethod.registerMethodClass(["HMAC-SHA1", "HMAC-SHA1-Accessor"],
			OAuth.SignatureMethod.makeSubclass(
				function getSignature(baseString) {
					b64pad = '=';
					var signature = b64_hmac_sha1(this.key, baseString);
					return signature;
				}
			));

			try {
				OAuth.correctTimestampFromSrc();
			} catch(e) {
			}

			/*
			* A JavaScript implementation of the Secure Hash Algorithm, SHA-1, as defined
			* in FIPS PUB 180-1
			* Version 2.1a Copyright Paul Johnston 2000 - 2002.
			* Other contributors: Greg Holt, Andrew Kepert, Ydnar, Lostinet
			* Distributed under the BSD License
			* See http://pajhome.org.uk/crypt/md5 for details.
			*/

			/*
			* Configurable variables. You may need to tweak these to be compatible with
			* the server-side, but the defaults work in most cases.
			*/
			var hexcase = 0;  /* hex output format. 0 - lowercase; 1 - uppercase        */
			var b64pad  = ""; /* base-64 pad character. "=" for strict RFC compliance   */
			var chrsz   = 8;  /* bits per input character. 8 - ASCII; 16 - Unicode      */

			/*
			* These are the functions you'll usually want to call
			* They take string arguments and return either hex or base-64 encoded strings
			*/
			function hex_sha1(s){return binb2hex(core_sha1(str2binb(s),s.length * chrsz));}
			function b64_sha1(s){return binb2b64(core_sha1(str2binb(s),s.length * chrsz));}
			function str_sha1(s){return binb2str(core_sha1(str2binb(s),s.length * chrsz));}
			function hex_hmac_sha1(key, data){ return binb2hex(core_hmac_sha1(key, data));}
			function b64_hmac_sha1(key, data){ return binb2b64(core_hmac_sha1(key, data));}
			function str_hmac_sha1(key, data){ return binb2str(core_hmac_sha1(key, data));}

			/*
			* Perform a simple self-test to see if the VM is working
			*/
			function sha1_vm_test()
			{
				return hex_sha1("abc") == "a9993e364706816aba3e25717850c26c9cd0d89d";
			}

			/*
			* Calculate the SHA-1 of an array of big-endian words, and a bit length
			*/
			function core_sha1(x, len)
			{
				/* append padding */
				x[len >> 5] |= 0x80 << (24 - len % 32);
				x[((len + 64 >> 9) << 4) + 15] = len;

				var w = Array(80);
				var a =  1732584193;
				var b = -271733879;
				var c = -1732584194;
				var d =  271733878;
				var e = -1009589776;

				for(var i = 0; i < x.length; i += 16)
				{
					var olda = a;
					var oldb = b;
					var oldc = c;
					var oldd = d;
					var olde = e;

					for(var j = 0; j < 80; j++)
					{
						if(j < 16) w[j] = x[i + j];
						else w[j] = rol(w[j-3] ^ w[j-8] ^ w[j-14] ^ w[j-16], 1);
						var t = safe_add(safe_add(rol(a, 5), sha1_ft(j, b, c, d)),
						safe_add(safe_add(e, w[j]), sha1_kt(j)));
						e = d;
						d = c;
						c = rol(b, 30);
						b = a;
						a = t;
					}

					a = safe_add(a, olda);
					b = safe_add(b, oldb);
					c = safe_add(c, oldc);
					d = safe_add(d, oldd);
					e = safe_add(e, olde);
				}
				return Array(a, b, c, d, e);

			}

			/*
			* Perform the appropriate triplet combination function for the current
			* iteration
			*/
			function sha1_ft(t, b, c, d)
			{
				if(t < 20) return (b & c) | ((~b) & d);
				if(t < 40) return b ^ c ^ d;
				if(t < 60) return (b & c) | (b & d) | (c & d);
				return b ^ c ^ d;
			}

			/*
			* Determine the appropriate additive constant for the current iteration
			*/
			function sha1_kt(t)
			{
				return (t < 20) ?  1518500249 : (t < 40) ?  1859775393 :
				(t < 60) ? -1894007588 : -899497514;
			}

			/*
			* Calculate the HMAC-SHA1 of a key and some data
			*/
			function core_hmac_sha1(key, data)
			{
				var bkey = str2binb(key);
				if(bkey.length > 16) bkey = core_sha1(bkey, key.length * chrsz);

				var ipad = Array(16), opad = Array(16);
				for(var i = 0; i < 16; i++)
				{
					ipad[i] = bkey[i] ^ 0x36363636;
					opad[i] = bkey[i] ^ 0x5C5C5C5C;
				}

				var hash = core_sha1(ipad.concat(str2binb(data)), 512 + data.length * chrsz);
				return core_sha1(opad.concat(hash), 512 + 160);
			}

			/*
			* Add integers, wrapping at 2^32. This uses 16-bit operations internally
			* to work around bugs in some JS interpreters.
			*/
			function safe_add(x, y)
			{
				var lsw = (x & 0xFFFF) + (y & 0xFFFF);
				var msw = (x >> 16) + (y >> 16) + (lsw >> 16);
				return (msw << 16) | (lsw & 0xFFFF);
			}

			/*
			* Bitwise rotate a 32-bit number to the left.
			*/
			function rol(num, cnt)
			{
				return (num << cnt) | (num >>> (32 - cnt));
			}

			/*
			* Convert an 8-bit or 16-bit string to an array of big-endian words
			* In 8-bit function, characters >255 have their hi-byte silently ignored.
			*/
			function str2binb(str)
			{
				var bin = Array();
				var mask = (1 << chrsz) - 1;
				for(var i = 0; i < str.length * chrsz; i += chrsz)
				bin[i>>5] |= (str.charCodeAt(i / chrsz) & mask) << (32 - chrsz - i%32);
				return bin;
			}

			/*
			* Convert an array of big-endian words to a string
			*/
			function binb2str(bin)
			{
				var str = "";
				var mask = (1 << chrsz) - 1;
				for(var i = 0; i < bin.length * 32; i += chrsz)
				str += String.fromCharCode((bin[i>>5] >>> (32 - chrsz - i%32)) & mask);
				return str;
			}

			/*
			* Convert an array of big-endian words to a hex string.
			*/
			function binb2hex(binarray)
			{
				var hex_tab = hexcase ? "0123456789ABCDEF" : "0123456789abcdef";
				var str = "";
				for(var i = 0; i < binarray.length * 4; i++)
				{
					str += hex_tab.charAt((binarray[i>>2] >> ((3 - i%4)*8+4)) & 0xF) +
					hex_tab.charAt((binarray[i>>2] >> ((3 - i%4)*8  )) & 0xF);
				}
				return str;
			}

			/*
			* Convert an array of big-endian words to a base-64 string
			*/
			function binb2b64(binarray)
			{
				var tab = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
				var str = "";
				for(var i = 0; i < binarray.length * 4; i += 3)
				{
					var triplet = (((binarray[i   >> 2] >> 8 * (3 -  i   %4)) & 0xFF) << 16)
					| (((binarray[i+1 >> 2] >> 8 * (3 - (i+1)%4)) & 0xFF) << 8 )
					|  ((binarray[i+2 >> 2] >> 8 * (3 - (i+2)%4)) & 0xFF);
					for(var j = 0; j < 4; j++)
					{
						if(i * 8 + j * 6 > binarray.length * 32) str += b64pad;
						else str += tab.charAt((triplet >> 6*(3-j)) & 0x3F);
					}
				}
				return str;
			}