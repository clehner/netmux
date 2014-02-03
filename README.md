# netmux

Turn interactive command line programs into network servers.

`netmux` lets you wrap a program with a socket server, so that multiple clients
can connect to the server and interact with the running program on standard I/O.

This is useful primarily if you have a program that responds immediately to each
line of input, and you want to share the program over a network with multiple
users.

See also [https://github.com/clehner/broadcat](broadcat) for a similar tool for
non-interactive programs.

## Installation

	make install

## Usage

	netmux [port] [command...]

## Example

Make a networked calculator:

	netmux 9898 bc -l

Connect to it:

	nc localhost 9898

## License

[MIT License](http://cel.mit-license.org/)
