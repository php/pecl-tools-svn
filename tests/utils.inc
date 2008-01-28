<?php

function delete_directory($directory)
{
	foreach (scandir($directory) as $file)
	{
		if ($file != '.' && $file != '..')
		{
			$file = $directory . DIRECTORY_SEPARATOR . $file;
			if (is_dir($file))
			{
				delete_directory($file);
			}
			else
			{
				unlink($file);
			}
		}
	}
	rmdir($directory);
}

?>