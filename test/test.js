const my_class = require('../index'),
	assert = require('assert');

let my_module = new my_class({highWaterMark: 1000});
try {
  my_module.on('data', function(chunk){
    console.log("chunk = ", chunk);
  });
  
} catch (err){
  console.log(err.message);
}
