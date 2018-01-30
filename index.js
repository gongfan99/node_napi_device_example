const { Readable } = require('stream'),
  simple_device = require('bindings')('simple-device-example');

class SourceWrapper extends Readable {
  constructor(options) {
    super(options);

    this._source = simple_device;

    // Every time there's data, push it into the internal buffer.
    let _this = this;
    this._source.onData(function(chunk){
      // if push() returns false, then stop reading from source
      let uint8View = new Uint8Array(chunk);
      console.log("onData = ", chunk, uint8View);
      if (!_this.push(chunk))
        _this._source.readStop();
    });

    // When the source ends, push the EOF-signaling `null` chunk
/*     this._source.onEnd(function(){
      this.push(null);
    }); */
  }
  // _read will be called when the stream wants to pull more data in
  // the advisory size argument is ignored in this case.
  _read(size) {
    console.log("_read started");
    this._source.readStart();
  }
}

module.exports = SourceWrapper;