const { Readable } = require('stream'),
  simple_device = require('bindings')('simple-device-example');

class SourceWrapper extends Readable {
  constructor(options) {
    super(options);

    this._source = simple_device;

    // Every time there's data, push it into the internal buffer.
    let _this = this;
    this._source.onData(function(err, chunk){
      // if push() returns false, then stop reading from source
      console.log("index.js: onData = ", chunk);
      if (!_this.push(chunk)){
        _this._source.readStop();
				console.log("index.js: buffer full, readStop() called");
			}
    });

    // When the source ends, push the EOF-signaling `null` chunk
    this._source.onEnd(function(){
      this.push(null);
    });
  }
  // _read will be called when the stream wants to pull more data in
  // the advisory size argument is ignored in this case.
  _read(size) {
    console.log("index.js: _read started");
    this._source.readStart();
  }
}

module.exports = SourceWrapper;