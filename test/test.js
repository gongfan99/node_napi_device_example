const my_class = require('../index'),
	assert = require('assert');

function delay(t){
	return new Promise(function(resolve, reject){
		setTimeout(function(){
			resolve(undefined);
		}, t);
	});
}

let my_module = new my_class({highWaterMark: 3});
(async function(){
	try {
		my_module.on('data', function(chunk){
			console.log("test.js: chunk = ", chunk);
		});
	
	await delay(1000);
	my_module.pause();
	console.log("test.js: paused");

	await delay(5000);
	} catch (err){
		console.log(err.message);
	}
})();