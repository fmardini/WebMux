require 'rubygems'
require 'haml'
require 'sinatra'

set :port, 8080
set :static, true

get "/" do
  haml :index
end
