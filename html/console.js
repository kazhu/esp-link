//===== Fetching console text

function fetchText(delay, repeat) {
  var el = $("#console");
  if (el.textEnd == undefined) {
    el.textEnd = 0;
    el.innerHTML = "";
  }
  window.setTimeout(function() {
    ajaxJson('GET', console_url + "?start=" + el.textEnd,
      function(resp) {
        var dly = updateText(resp);
        if (repeat) fetchText(dly, repeat);
      },
      function() { retryLoad(repeat); });
  }, delay);
}

function updateText(resp) {
  var el = $("#console");

  var delay = 3000;
  if (resp != null && resp.len > 0) {
    var isScrolledToBottom = el.scrollHeight - el.clientHeight <= el.scrollTop + 1;

    // append the text
    if (resp.start > el.textEnd) {
      el.innerHTML = el.innerHTML.concat("\r\n<missing lines\r\n");
    }
    el.innerHTML = el.innerHTML.concat(resp.text
       .replace(/&/g, '&amp;')
       .replace(/</g, '&lt;')
       .replace(/>/g, '&gt;')
       .replace(/"/g, '&quot;'));
    el.textEnd = resp.start + resp.len;
    delay = 500;

    // scroll to bottom
    if(isScrolledToBottom) el.scrollTop = el.scrollHeight - el.clientHeight;
  }
  return delay;
}

function retryLoad(repeat) {
  fetchText(1000, repeat);
}
