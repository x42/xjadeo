(function() {
  var $ = jQuery;

  $('a.hover').find('img').each(function() {
    var $img = $( this );
    $img.attr('data-orig', $img.attr('src'));
    var preloader = new Image();
    preloader.src = $img.attr('data-hover');
  });

  $('body').on('mouseover', 'img[data-hover]', function(event) {
    var $img = $(event.currentTarget);
    $img.attr('src', $img.attr('data-hover'));
    return false;
  });

  $('body').on('mouseout', 'img[data-hover]', function(event) {
    var $img = $(event.currentTarget);
    $img.attr('src', $img.attr('data-orig'));
    return false;
  });

}).call(this);
