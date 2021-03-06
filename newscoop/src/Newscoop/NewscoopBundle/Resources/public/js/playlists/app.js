(function() {
'use strict';
var app = angular.module('playlistsApp', [
    'ngSanitize',
    'ui.select',
    'ngTable',
    'ng-sortable',
    'ui.bootstrap',
    'infinite-scroll',
    'ngActivityIndicator'
]).config(function($interpolateProvider, $httpProvider) {
    $interpolateProvider.startSymbol('{[{').endSymbol('}]}');
    $httpProvider.interceptors.push('authInterceptor');
});
})();