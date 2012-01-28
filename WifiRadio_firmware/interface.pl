#!/usb/packages/usr/bin/perl -w

system("/usb/packages/usr/bin/stty 9600 -echo < /dev/tts/1");

sub sendTracks($$)
{
    ($currentDir, $currentListStartIndex) = @_;
    
    $currentListStartIndex += 4;
        
    $cmd = "./mpc ls $currentDir | head -n $currentListStartIndex | tail -n 4";
    @trackList = `$cmd`;
                    
    open(WRITER, ">response");
    foreach(@trackList)
    {
        $index = rindex($_, '/');
        if($index >= 0)
        {
            $trackDirName = substr($_, $index+1, length($_) - $index);
        }
        else
        {
           $trackDirName = $_;
        }
        print WRITER $trackDirName;
    }
    close(WRITER);
}
                                                                                                                                

if (fork) 
{
    	while(1)
	{
		$totalString = "";
	
		if(-e "response")
		{
			open READER, "<", "response"; 

			while (<READER>) 
			{ 
				$line = $_;
				chomp($line);
				$totalString .= substr($line, 0, 19);
				$totalString .= ","; 
			}
			close(READER);
			unlink("response");
			$totalString = "resp: ".$totalString;	
		}
		else
		{
			@songInfo = `echo "currentsong" | nc localhost 6600`;
			@statusInfo = `echo "status" | nc localhost 6600`;	
			
			chomp(@songInfo);
			foreach(@songInfo)
			{
				if($_ =~ /^Name: / or $_ =~ /^Artist: / or $_ =~ /^Title: /)
				{
					$totalString .= substr($_,0,28);
					$totalString .= " "; 
				}
			}
	
			chomp(@statusInfo);
			foreach(@statusInfo)
			{
		                if($_ =~ /^time: / or $_ =~ /^playlistlength: / or $_ =~ /^song: /)
		                {
		                	$totalString .= $_;
					$totalString .= " "; 
		                }
			}
		}
			
		$totalString =~ s/\'//;	
		
		print "Sending: ".$totalString."\n";
			
		$commandString = 'echo \''.$totalString.'\' > /dev/tts/1'."\n";
			
		system($commandString);
		
		sleep(1);
	}
}
else
{
	@currentDir = ();
	while(1)
	{
		$command = `head -n 1 < /dev/tts/1`;
		$command =~ s/^cmd://;
		chomp($command);
		
		print "Received: ".$command."\n";
		
		if($command eq "getfirsttracks")
		{
		    `./mpc stop`;
		    @currentDir = ();
		    @trackList = `./mpc ls | head -n 4`;
		                                                                                                                 
		    open(WRITER, ">response");
		    foreach(@trackList)
		    {
	                print WRITER $_;
                    }
	            close(WRITER);
                }
		                                                                                                                                                                                                                                                                                         
		if($command =~ m/^gettracks\s(\d+)\s(\d+)/)
		{
		    $currentListStartIndex = $1;
		    $currentDir = join "/",@currentDir;
			
		    sendTracks($currentDir, $currentListStartIndex);
		}
	
	        if($command =~ m/^play\s(\d+)\s(\d+)/)
	        {
	            $currentListStartIndex = $1;
	            $currentListSelectedIndex = $2;
	            
                    $currentDir = join "/",@currentDir;
                    @trackList = `./mpc ls $currentDir`;
                    
                    $entryToPlay .= $trackList[$currentListStartIndex + $currentListSelectedIndex];
                    $entryToPlay =~ s/ /\\ /g;
                   
                    `./mpc clear`;
                    `./mpc add $entryToPlay`;
                    `./mpc play`;
	        }
		
		if($command =~ m/^dirdown\s(\d+)\s(\d+)/)
		{
                    $currentListStartIndex = $1;
                    $currentListSelectedIndex = $2;

                    $currentDir = join "/",@currentDir;
                    @trackList = `./mpc ls $currentDir`;

                    $newDir = $trackList[$currentListStartIndex + $currentListSelectedIndex];
                    chomp($newDir);
                    $index = rindex($newDir, '/');
                    if($index >= 0)
                    {
                        $trackDirName = substr($newDir, $index+1, length($newDir) - $index);
                    }
                    else
                    {
                        $trackDirName = $newDir;
                    }
	            $trackDirName =~ s/ /\\ /g;
                    
                    push(@currentDir, $trackDirName);

                    $currentDir = join "/",@currentDir;
                    sendTracks($currentDir, 0);
		}
		
		if($command eq "dirup")
		{
		    pop(@currentDir);
		    
		    $currentDir = join "/", @currentDir;
		    sendTracks($currentDir, 0);
		}
	
		if($command eq "next")	
		{
			`./mpc next`;
		}
		                   
		if($command eq "prev")	
		{
		        `./mpc prev`;
		}
		                                         
		if($command eq "volup")	
		{
			`./mpc volume +5`;
		}
		                                                               
		if($command eq "voldown")	
		{
			`./mpc volume -5`;
                }
		
		if($command eq "loadstreams")
		{
	                `./mpc clear`;
                        `./mpc repeat on`;
                           
                        `./mpc add http://205.188.215.232:8016                         `;    # di.fm Soulful House
                        `./mpc add http://scfire-ntc-aa03.stream.aol.com:80/stream/1009`;    # di.fm Lounge
                        `./mpc add http://205.188.215.225:8002                         `;    # di.fm Breaks
                        `./mpc add http://scfire-ntc-aa03.stream.aol.com:80/stream/1025`;    # di.fm Electro House
                                                           
                        `./mpc playlist`;    # show the resulting playlist
                                                                  
                	`./mpc play`;
        	}
                                                                
	}
}
