
clear all;
close all;
clc;   % position the cursor at the top of the screen
%clf;   %  closes the figure window

% -------------------------------------------------------------------

path = '';
fig = 0;
hf = zeros(1,4);
xLimit = 1000;

for s=1:1    
    
    if(s == 1)
        path = '../results/gui/speed-gap.txt';
        figureName = 'Optimal Speed';
    elseif(s == 2)
        path = '../2.Result/speed-gap-21MSCFModel_KraussFixed.txt';
        figureName = 'Krauss Fixed';
    elseif(s == 3)
        path = '../2.Result/speed-gap-21MSCFModel_KraussOrig1.txt';
        figureName = 'KraussOrig1';
    end
    
    file_id = fopen(path);
    formatSpec = '%d %f %s %s %s %f %f %f %s %f %f';
    C_text = textscan(file_id, formatSpec, 'HeaderLines', 3);
    fclose(file_id);

    % ---------------------------------------------------------------
    
    indices = C_text{1,1};
    timeSteps = C_text{1,2};
    vehicles = C_text{1,3};    
    speeds = C_text{1,7};
    accel = C_text{1,8};
    timeGaps = C_text{1,11};

    % ---------------------------------------------------------------
    
    % stores vehicle IDs
    vIDs = {};

    [rows,~] = size(vehicles);
    n = indices(end,1);

    % preallocating and initialization with -1
    vehiclesTS = zeros(n,1) - 1;
    vehiclesSpeed = zeros(n,1) - 1;
    vehiclesAccel = zeros(n,1) - 1;
    vehiclesTimeGap = zeros(n,1) - 1;

    for i=1:rows   
        
        index = int32(indices(i,1));        

        % store the current TS
        vehiclesTS(index,1) = double(timeSteps(i,1));
    
        % get the current vehicle name
        vehicle = char(vehicles(i,1));
        
        vNumber = find(ismember(vIDs,vehicle));        
        if( isempty(vNumber) )
            vIDs{end+1} = vehicle;
            [~,vNumber] = size(vIDs);
        end   

        vehiclesSpeed(index,vNumber) = speeds(i,1); 
        vehiclesAccel(index,vNumber) = accel(i,1);
        vehiclesTimeGap(index,vNumber) = timeGaps(i,1);  
    end
    
    [~, VehNumbers] = size(vIDs);

    % ---------------------------------------------------------------
    
    % speed profile of vehicles    

    figure('units','normalized','outerposition',[0 0 1 1]);
    fig = fig + 1;
    hf(fig) = figure(fig);
    set(gcf,'name','Speed');
    % set(gcf,'name','Myfigure','numbertitle','off')

    handle1 = plot(vehiclesTS,vehiclesSpeed,'LineWidth', 3);

    % set the x-axis limit
    set( gca, 'XLim', [0 xLimit] );
    
    % set the y-axis limit
    set( gca, 'YLim', [0 35] );

    % set font size
    set(gca, 'FontSize', 19);

    % set the axis labels
    xlabel('Simulation Time (s)', 'FontSize', 19);
    ylabel('Speed (m/s)', 'FontSize', 19);

    grid on;
    
    % speed profile of first vehicle (dashed line)
    set(handle1(1), 'LineStyle', '-.');
    
    [n , ~] = size(handle1);
    
    %{
    color = 0;
    
    for j=2:n
        set(handle1(j), 'Color', hsv2rgb([0 0 color]) );     
        color = color + 0.111;
    end   
    %}
    
    % set the name for each line
    for i=1:VehNumbers
        name = sprintf('veh %2d', i);
        set(handle1(i),'Displayname', name);   
    end
    
    % set the legend
    legend(handle1, 'Location','NorthEastOutside');    

    % save the figure as fig to restore it later
    % saveas(gcf,'figure1.fig');

    % save the figure as a png file
    set(gcf, 'PaperPositionMode', 'auto'); 
    figName = sprintf('figure%d',fig);
    print('-dpng', '-r300', figName);

    % --------------------------------------------------------------
    
    % time gap of vehicles 

    figure('units','normalized','outerposition',[0 0 1 1]);
    fig = fig + 1;
    hf(fig) = figure(fig);
    set(gcf,'name','Time Gap');

    handle2 = plot(vehiclesTS,vehiclesTimeGap,'LineWidth', 3);

    % set the x-axis limit
    set( gca, 'XLim', [0 xLimit] );
    
    % set the y-axis limit
    %set( gca, 'YLim', [0 120] );

    % set font size
    set(gca, 'FontSize', 19);

    % set the axis labels
    xlabel('Simulation Time (s)', 'FontSize', 19);
    ylabel('TimeGap (m)', 'FontSize', 19);

    grid on;   
    
    % speed profile of first vehicle (dashed line)
    set(handle2(1), 'LineStyle', '-.');
    
    [n , ~] = size(handle2);
    
    % set the name for each line
    for i=1:VehNumbers
        name = sprintf('veh %2d', i);
        set(handle2(i),'Displayname', name);   
    end    
    
    % set the legend
    legend(handle2, 'Location','NorthEastOutside');

    % save the figure as a png file
    set(gcf, 'PaperPositionMode', 'auto');
    figName = sprintf('figure%d',fig);
    print('-dpng', '-r300', figName);
    
    % ---------------------------------------------------------------
    
    % acceleration of vehicles 
        
    figure('units','normalized','outerposition',[0 0 1 1]);
    fig = fig + 1;
    hf(fig) = figure(fig);
    set(gcf,'name','Acceleration');

    handle3 = plot(vehiclesTS,vehiclesAccel,'LineWidth', 3);

    % set the x-axis limit
    set( gca, 'XLim', [0 xLimit] );
    
    % set the y-axis limit
   % set( gca, 'YLim', [0 30] );

    % set font size
    set(gca, 'FontSize', 19);

    % set the axis labels
    xlabel('Simulation Time (s)', 'FontSize', 19);
    ylabel('Acceleration (m/s^2)', 'FontSize', 19);

    grid on;
    
    % set the name for each line
    for i=1:VehNumbers
        name = sprintf('veh %2d', i);
        set(handle3(i),'Displayname', name);   
    end    
    
    % set the legend
    legend(handle3, 'Location','NorthEastOutside');

    % save the figure as fig to restore it later
    % saveas(gcf,'figure1.fig');

    % save the figure as a png file
    set(gcf, 'PaperPositionMode', 'auto'); 
    figName = sprintf('figure%d',fig);
    print('-dpng', '-r300', figName);    
        
end

% -------------------------------------------------------------------

%figs2subplots([hf(1) hf(7)],[2 1],{1,2});


%figs2subplots([hf(4) hf(7)],[2 1],{1,2});


