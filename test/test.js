const my_module = require('bindings')('simple-device-example'),
	assert = require('assert');

try {
  my_module.onData(function(err, chunk){
    console.log("chunk = ", chunk);
  });
  
  my_module.readStart();
  
  setTimeout(function(){
    my_module.readStop();
  }, 3000)

} catch (err){
  console.log(err.message);
}
