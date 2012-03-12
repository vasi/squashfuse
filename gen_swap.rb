#!/usr/bin/ruby

hdr = open('squashfs_fs.h')
genh = open('swap.h.inc', 'w')
genc = open('swap.c.inc', 'w')

func = nil
hdr.each do |line|
	if func.nil?
		if line.match(/^\s*struct\s+(squashfs_(\w+))\s*\{\s*$/)
			func = "sqfs_swapin_#$2"
			decl = "void #{func}(struct #$1 *s)"
			genh.puts "#{decl};"
			genc.puts "#{decl} {"
		end
	elsif line.match(/^\s*\}\s*;\s*$/)
		genc.puts "}"
		func = nil
	elsif md = line.match(/^\s*__le(\d+)\s+(\w+)\s*;\s*$/)
		# ignore unknown fields
		genc.puts "sqfs_swapin#$1(&s->#$2);"
	end
end
