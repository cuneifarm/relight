function formatMm(val) {
	if(val < 20)
		return val.toFixed(1) + " mm";
	if(val < 500)
		return (val/10).toFixed(1) + " cm";
	if(val < 100000)
		return (val/1000).toFixed(1) + " m";
	else
		return (val/1000000).toFixed(2) + " km";
}

function RelightViewer(div, options) {

	var t = this;
	if(typeof(div) == "string")
		div = document.querySelector(div);
	t.div = div;

	t.nav = {
		action:null, lighting: false, fullscreen:false, 
		pandelay: 50, zoomdelay:200, zoomstep: 0.25, lightsize:0.8,
		pointers: {}, 
		support: support,
		pagemap: { size: 200 },
		actions: {
			home:    { title: 'Home',       task: function(event) { t.centerAndScale(t.nav.zoomdelay); }        },
			zoomin:  { title: 'Zoom In',    task: function(event) { t.zoom(-t.nav.zoomstep, t.nav.zoomdelay); } },
			zoomout: { title: 'Zoom Out',   task: function(event) { t.zoom(+t.nav.zoomstep, t.nav.zoomdelay); } },
			light:   { title: 'Light',      task: function(event) { t.toggleLight(event); }                      },
			full:    { title: 'Fullscreen', task: function(event) { t.toggleFullscreen(event); }                 },
			info:    { title: 'info',       task: function(event) { t.showInfo(); }                             }
		},
		scale: 0                     //size of a pixel in mm.
	};

	if(options.hasOwnProperty('notool'))
		for(var i = 0; i < options.notool.length; i++)
			delete t.nav.actions[options.notool[i]];


	for(var i in t.nav)
		if(options.hasOwnProperty(i))
			t.nav[i] = options[i];

	var html = 
'	<canvas id="rticanvas"></canvas>\n' +
'	<div class="relight-toolbox">\n';
	for(var i in t.nav.actions) {
		var action = t.nav.actions[i];
		html += '		<div class="relight-' + i + '" title="' + action.title + '"></div>\n';
	}

	html += '	</div>\n';
	if(t.nav.scale)
		html += '	<div class="relight-scale"><hr/><span></span></div>\n';

	if(t.nav.pagemap)
		html += '	<div class="relight-pagemap"><div class="relight-pagemap-area"></div>\n';

	html += '	<div class="relight-info-dialog"></div>\n';

	var info = document.querySelector(".relight-info-content");
	if(info)
		info.remove();

	div.innerHTML = html;

	t.dialog = div.querySelector(".relight-info-dialog");

	if(info) {
		t.dialog.appendChild(info);
		info.style.display = 'block';
		t.dialog.onclick = function() { t.hideInfo(); };
	}

	if(t.nav.pagemap) {
		t.nav.pagemap.div  = document.querySelector(".relight-pagemap");
		t.nav.pagemap.area = document.querySelector(".relight-pagemap-area");
	}

	for(var i in t.nav.actions)
		t.addAction('.relight-' + i, t.nav.actions[i].task);

	var canvas = document.querySelector('#rticanvas');

	Relight.call(this, canvas, options);

	var support = "onwheel" in document.createElement("div") ? "wheel" : // Modern browsers 
		document.onmousewheel !== undefined ? "mousewheel" : // Webkit and IE support at least "mousewheel"
		"DOMMouseScroll"; // older Firefox
	t.canvas.addEventListener(support,   function(e) { t.mousewheel(e); },   false);
	window.addEventListener('resize', function(e) { t.resize(canvas.offsetWidth, canvas.offsetHeight); });

	var mc = new Hammer.Manager(t.canvas);
	mc.add( new Hammer.Pan({ direction: Hammer.DIRECTION_ALL, threshold: 0 }) );
	mc.on('panstart',         function(ev) { t.mousedown(ev); });
	mc.on('panmove',          function(ev) { t.mousemove(ev); });
	mc.on('panend pancancel', function(ev) { t.mouseup(ev); });

	mc.add( new Hammer.Pinch() );
	mc.on('pinchstart',           function(ev) { t.mousedown(ev); });
	mc.on('pinchmove',            function(ev) { t.mousemove(ev); });
	mc.on('pinchend pinchcancel', function(ev) { t.mouseup(ev); });

	mc.add( new Hammer.Tap({ taps:2 }) );
	mc.on('tap', function(ev) { t.zoom(-2*t.nav.zoomstep, t.nav.zoomdelay); });

	t.resize(canvas.offsetWidth, canvas.offsetHeight);
	if(options.scale)
		t.onPosChange(function() { t.updateScale(); });
	//remember size (add exif into json!)
	if(t.nav.pagemap) {
		t.onLoad(function() { t.initPagemap(); });
		t.onPosChange(function() { t.updatePagemap(); });
	}
}


RelightViewer.prototype = Relight.prototype;

RelightViewer.prototype.addAction = function(selector, action) {
	var tap = new Hammer.Manager(document.querySelector(selector));
	tap.add(new Hammer.Tap());
	tap.on('tap', action);
};

RelightViewer.prototype.toggleLight = function(event) {
	var t = this;
	if(t.nav.lighting)
		event.target.classList.remove('relight-light_on');
	else
		event.target.classList.add('relight-light_on');

	t.nav.lighting = !t.nav.lighting; 
};

RelightViewer.prototype.toggleFullscreen = function(event) {
	var t = this;
	var div = t.div;
	if(t.nav.fullscreen) {
		var request = document.exitFullscreen || document.webkitExitFullscreen ||
			document.mozCancelFullScreen || document.msExitFullscreen;
		request.call(document);
		event.target.classList.remove('relight-full_on');

		div.style.height = "100%";
		t.resize(t.canvas.offsetWidth, t.canvas.offsetHeight);
	} else {
		var request = div.requestFullscreen || div.webkitRequestFullscreen ||
			div.mozRequestFullScreen || div.msRequestFullscreen;
		request.call(div);
		event.target.classList.add('relight-full_on');
	}
	div.style.height = window.offsetHeight + "px";
	t.resize(t.canvas.offsetWidth, t.canvas.offsetHeight);

	t.nav.fullscreen = !t.nav.fullscreen; 
};

RelightViewer.prototype.updateScale = function() {
	var t = this;
	var span = t.div.querySelector('.relight-scale > span');
	var scale = Math.pow(2, t.pos.z);
	var scalesize = t.options.scale*100*scale; //size of a pixel
	span.innerHTML = formatMm(scalesize); 
};

RelightViewer.prototype.initPagemap = function() {
	var t = this;
	var page = t.nav.pagemap;
	var w = t.width;
	var h = t.height;
	if(w > h) {
		page.w = page.size;
		page.h = page.size * h/w;
	} else {
		page.w = page.size * w/h
		page.h = page.size;
	}
	page.div.style.width =  page.w+  'px';
	page.div.style.height = page.h + 'px';
	
	page.area.style.width =  page.w/2+  'px';
	page.area.style.height = page.h/2 + 'px';
	t.updatePagemap();
}

RelightViewer.prototype.updatePagemap = function() {
	var t = this;
	var page = t.nav.pagemap;
	var a = page.area;
	if(!page.w) return;

	var w = t.canvas.width;
	var h = t.canvas.height;

	var scale = Math.pow(2, t.pos.z);
	var bbox = [
		Math.max(0, parseInt((t.pos.x - scale*w/2)/t.width* page.w)),
		Math.max(0, parseInt((t.pos.y - scale*h/2)/t.height*page.h)),
		Math.min(page.w, parseInt((t.pos.x + scale*w/2)/t.width* page.w)),
		Math.min(page.h, parseInt((t.pos.y + scale*h/2)/t.height*page.h))
	];

	page.area.style.left = bbox[0] + 'px';
	page.area.style.top =  bbox[1] + 'px';
	page.area.style.width = (bbox[2] - bbox[0]) + 'px';
	page.area.style.height = (bbox[3] - bbox[1]) + 'px';
}


RelightViewer.prototype.mousedown = function(event) {
	var t = this;
	var src = event.srcEvent
	if(event.type == 'pinchstart')
		t.nav.action = 'zoom';
	else if(t.nav.lighting || src.shiftKey || src.ctrlKey || src.button > 0) {
		t.nav.action = 'light';
		t.lightDirection(event);
	} else
		t.nav.action = 'pan';

	t.nav.pos = this.pos;
	t.nav.light = this.light;
};

RelightViewer.prototype.lightDirection = function(event) {
	var t = this;
	var e = event.srcEvent;
	var w = t.nav.lightsize*t.canvas.width/2;
	var h = t.nav.lightsize*t.canvas.height/2;

	var x = (e.offsetX - t.canvas.width/2)/w;
	var y = (e.offsetY - t.canvas.height/2)/h;

	var r = Math.sqrt(x*x + y*y);
	if(r > 1.0) {
		x /= r;
		y /= r;
		r = 1.0;
	}
	var z = Math.sqrt(1 - r);
	t.setLight(-x, y, z);
}

RelightViewer.prototype.mousemove = function(event) {
	var t = this;
	if(!t.nav.action) return;

	var p = t.nav.pos;
	var x = event.deltaX;
	var y = event.deltaY;

	switch(t.nav.action) {
	case 'pan':
		var scale = Math.pow(2, p.z);
		x *= scale;
		y *= scale;
		t.setPosition(p.x - x, p.y - y, p.z, t.nav.pandelay);
		break;

	case 'zoom':
		var z = p.z/Math.pow(event.scale, 1.5);
		t.setPosition(p.x, p.y, z, t.nav.zoomdelay);
		break;

	case 'light':
		t.lightDirection(event);
		break;
	}
};

RelightViewer.prototype.mouseup = function(event) {
	if(this.nav.action) {
		this.nav.action = null;
		event.preventDefault();
	}
};

RelightViewer.prototype.mousewheel = function(event) {
	if ( this.nav.support == "mousewheel" ) {
		event.deltaY = - 1/40 * event.wheelDelta;
	} else {
		event.deltaY = event.deltaY || event.detail;
	}

	var dz = event.deltaY > 0? this.nav.zoomstep : -this.nav.zoomstep;
	this.zoom(-dz, this.nav.zoomdelay);
	event.preventDefault();
};


RelightViewer.prototype.setInfo = function(info) {
	if(typeof(info) == "string")
		this.dialog.innerHTML = info;
	else
		this.dialog.append(info);
};

RelightViewer.prototype.showInfo = function() {
	this.dialog.style.display = 'block';
};

RelightViewer.prototype.hideInfo = function() {
	this.dialog.style.display = 'none';
};


