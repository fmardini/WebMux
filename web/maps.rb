require 'rubygems'
require 'haml'
require 'sinatra'

set :port, 8080
set :static, true
enable :sessions, :logging

get "/" do
  haml :index
end
